# -*- coding: utf-8 -*-
import torch
import torch.nn as nn
from torch.nn import init
import torch.nn.functional as F
import torch.optim as optim
import numpy as np
import pandas as pd
import argparse
import os, sys, random, time
from itertools import count
import socket
import math
import struct
from tensorboardX import SummaryWriter
from torch.distributions import Categorical
import csv
from torch.autograd import Variable

_HOST = 'localhost'
_PORT = 10001

tcpServer = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
tcpServer.bind((_HOST, _PORT))
tcpServer.listen(5)

device = 'cuda' if torch.cuda.is_available() else 'cpu'
directory = './testDir/'
reward_type = 'ThroughputDelay'

window_size = 5 

first_flag = True
cur_state = np.array([])
last_state = np.array([])
# print ('Cur_state: ', cur_state.shape[0])
action = [0.0005]
score = 0.0
best_throughput_train = []
best_throughput_test = []
best_delay_train = []
best_delay_test = []
best_length_train = []
best_length_test = []
best_enqueue_train = []
best_enqueue_test = []
last_retran = 0
last_drop = 0
last_reward = 0
reward_list = []
throughput_list = []
delay_list = []
length_list = []
enqueue_list = []
score_list_train = []
score_list_test = []
max_score_train = 0
max_score_test = 0
best_tag = False
eps = np.finfo(np.float32).eps.item()
min_thre_list = []
max_thre_list = []
average_reward = 0.0
sum_reward = 0.0
count_reward = 0
horizon = 3
state_dim = 5
action_dim = 100
learning_rate = 0.001
update_iteration = 5
tau = 0.01
action_bias = 0.0015
gamma = 0.9
batch_size = 128
log_interval = 10
capacity = 500
epoch = 100
epsilon = 0.1
forward_steps = 200 - window_size
ppo_clip_param = 0.1
entropy_coef = 0.1


def weights_init_normal(m):
    '''Takes in a module and initializes all linear layers with weight
       values taken from a normal distribution.'''

    classname = m.__class__.__name__
    # for every Linear layer in a model
    # m.weight.data shoud be taken from a normal distribution
    # m.bias.data should be 0
    if classname.find('Linear') != -1:
        m.weight.data.normal_(0.0, 0.2)
        m.bias.data.fill_(0)
    if isinstance(m, nn.LSTM):
        init.xavier_normal_(m.all_weights[0][0])
        init.xavier_normal_(m.all_weights[0][1])
        #init.xavier_normal_(m.all_weights[1][0])
        #init.xavier_normal_(m.all_weights[1][1])
        # init.normal_(m.weight.data, 0, 0.05)
        # init.constant_(m.bias.data, 0.1)
    # init.xavier_normal(m.weight.data)


class RolloutStorage(object):
    def __init__(self, num_steps, state_dim, action_dim):
        self.states = torch.zeros(num_steps + 1, window_size, state_dim)
        self.rewards = torch.zeros(num_steps, 1)
        self.action_log_probs = torch.zeros(num_steps, 1)
        self.returns = torch.zeros(num_steps + 1, 1)
        self.value_preds = torch.zeros(num_steps + 1, 1)
        self.actions = torch.zeros(num_steps, 1).long()
        # self.hstates = torch.zeros(num_steps + 1, state_dim)
        self.step = 0
        self.num_steps = num_steps

    def insert(self, step, state, action, action_log_prob, value_pred, reward):
        state = torch.FloatTensor(state)
        action = torch.LongTensor(action)
        action_log_prob = torch.FloatTensor(action_log_prob)
        value_pred = torch.FloatTensor(value_pred)
        reward = torch.FloatTensor([reward])
        self.states[step + 1].copy_(state)
        self.actions[step].copy_(action)
        self.action_log_probs[step].copy_(action_log_prob)
        self.value_preds[step].copy_(value_pred)
        self.rewards[step].copy_(reward)
        # self.hstates[step + 1].copy_(hstate)

    def initialize(self, state):
        self.states = torch.zeros(self.num_steps + 1, window_size, state_dim)
        self.rewards = torch.zeros(self.num_steps, 1)
        self.action_log_probs = torch.zeros(self.num_steps, 1)
        self.returns = torch.zeros(self.num_steps + 1, 1)
        self.value_preds = torch.zeros(self.num_steps + 1, 1)
        self.actions = torch.zeros(self.num_steps, 1).long()
        state = torch.FloatTensor(state)
        self.states[0].copy_(state)

    def compute_returns(self, next_value, gamma, tau):
        next_value = torch.FloatTensor(next_value)
        self.value_preds[-1] = next_value
        gae = 0
        for step in reversed(range(self.rewards.size(0))):
            delta = self.rewards[step] + gamma * self.value_preds[step + 1] - self.value_preds[step]
            gae = delta + gamma * tau * gae
            self.returns[step] = gae + self.value_preds[step]

    def sample(self):
        return self.states[:-1], self.actions, self.returns[:-1], self.action_log_probs
    
    def get_batch(self, batch_ix):
        return self.states[batch_ix], self.returns[batch_ix], self.actions[batch_ix], self.action_log_probs[batch_ix], self.value_preds[batch_ix]


class RNNBase(nn.Module):
    def __init__(self, state_dim, action_dim):
        super(RNNBase, self).__init__()
        hidden_dim = 5 
        model_output_dim = 10
        self.lstm = nn.LSTM(input_size=state_dim, hidden_size=hidden_dim, bidirectional=False, batch_first=True)
        # init_ = lambda m: init(m, nn.init.orthogonal_, lambda x: nn.init.constant_(x, 0), np.sqrt(2))
        self.actor = nn.Sequential(
                        nn.Linear(hidden_dim, 50), nn.Tanh(),
                        nn.Linear(50, 40), nn.Tanh(),
                        nn.Linear(40, action_dim)
                        )
        self.critic = nn.Sequential(
                        nn.Linear(hidden_dim, 50), nn.Tanh(),
                        nn.Linear(50, 40), nn.Tanh(),
                        nn.Linear(40, 1)
                        )

    def forward(self, x):
        output, (h_n, c_n) = self.lstm(x)
        x = output[:, -1, :]
        critic = self.critic(x)
        actor = self.actor(x)
        return critic, actor


class A2C(nn.Module):
    def __init__(self, state_dim, action_dim):
        super(A2C, self).__init__()
        self.base = RNNBase(state_dim, action_dim)
        self.deterministic = False

    def act(self, inputs, mode):
        value, actor_features = self.base(inputs)
        actor_features = F.softmax(actor_features)
        dist = Categorical(actor_features)
        if self.deterministic:
            action = dist.mode()
        else:
            action = dist.sample()
        action_log_probs = dist.log_prob(action)
        if mode == 'Training':
            if random.random() < epsilon:
                action = [np.random.choice(range(action_dim))]
        return value, action, action_log_probs

    def get_value(self, inputs):
        value, _ = self.base(inputs)        
        return value
        
    def evaluate_actions(self, inputs):
        value, actor_features = self.base(inputs)
        # actor_features = F.softmax(actor_features)
        return value, actor_features
    
    # def select_action(self, state):
    #     state = torch.FloatTensor(state.reshape(1, window_size, -1)).to(device)
    #     return self.actor.predict(state).cpu().data.numpy().flatten()

def get_critic_loss(old_state_estimates_b, state_estimates, returns_b):
    state_estimates = state_estimates.squeeze(-1)
    clipped_state_estimate = old_state_estimates_b + torch.clamp(state_estimates - old_state_estimates_b, -.2, .2)
    critic_loss_21 = ((returns_b - clipped_state_estimate)**2)
    critic_loss_2 = ((returns_b - state_estimates)**2)
    critic_loss = torch.max(critic_loss_2, critic_loss_2)
    critic_loss = critic_loss.mean() * .5
    return critic_loss
    
def calculate_action_gain_ppo(action_probs, old_action_log_probs, actions_taken, advantages):
    action_probs = F.log_softmax(action_probs)
    chosen_action_probs = action_probs.gather(1, actions_taken)
    ratio = torch.exp(chosen_action_probs - old_action_log_probs)
    unclipped_action_gain = ratio * advantages
    clipped_action_gain = torch.clamp(ratio, .8, 1.2) * advantages
    action_gain = torch.min(unclipped_action_gain, clipped_action_gain)
    action_gain = action_gain.mean()
    return action_gain

def get_entropy_bonus(logits):
    action_probs = F.log_softmax(logits)
    e = -(action_probs.exp() * (action_probs + 1e-8))
    e = e.sum(-1)
    e = e.mean()
    return e
    
def run_batch(batch_ix):
    state_b, return_b, action_b, old_action_log_probs_b, value_pred_b = rollouts.get_batch(batch_ix)
    values, actor_features = model.evaluate_actions(state_b)
    entropy_bonus = get_entropy_bonus(actor_features)
    critic_loss = get_critic_loss(value_pred_b, values, return_b)
    with torch.no_grad():
        advantages = (return_b - values.squeeze(-1).detach())
        advantages -= advantages.mean()
        advantages /= (advantages.std() + 1e-8)
    action_gain = calculate_action_gain_ppo(actor_features, old_action_log_probs_b, action_b, advantages)
    return entropy_bonus, action_gain, critic_loss

def update_model():
    global forward_steps
    # hx, cx = rollouts.hstates[-1].split(state_dim, 1)
    max_grad_norm = None
    state = rollouts.states[-1].reshape(1, window_size, -1)
    next_value = model.get_value(state)
    rollouts.compute_returns(next_value[0], gamma, tau)
    batch_size = 10
    batch_num = (forward_steps) / batch_size
    batch_num = int(batch_num)
    temp = np.array(range(forward_steps))
    np.random.shuffle(temp)
    for i in range(batch_num):
        print ('batch_num: ', i)
        batch_ix = []
        for j in range(batch_size * i, batch_size * (i + 1)):
            batch_ix.append(temp[j])
        entropy_bonus, action_gain, critic_loss = run_batch(batch_ix)
        entropy_bonus *= entropy_coef
        total_loss = critic_loss - entropy_bonus - action_gain
        if i != (batch_num - 1): total_loss.backward(retain_graph=True)
        optimizer.step()
        optimizer.zero_grad()
    
    # run_batch(batch_ix)
    # state_b, action_b, return_b, old_action_log_probs_b = rollouts.sample()
    # values, actor_features = model.evaluate_actions(state_b)
    # action_log_probs, dist_entropy = get_entropy(actor_features, action_b)
    # advantages = rollouts.returns[:-1] - rollouts.value_preds[:-1]
    # value_loss = advantages.pow(2).mean()
    # action_loss = -(advantages.detach() * action_log_probs).mean()
    # optimizer.zero_grad()
    # loss = (value_loss + action_loss - dist_entropy * entropy_coef)
    # print ('Loss', value_loss, action_loss, dist_entropy)
    # loss.backward()
    # nn.utils.clip_grad_norm(model.parameters(), max_grad_norm)
    # optimizer.step()


# ---------------------------Loading Models----------------------------#
model = A2C(state_dim, action_dim)
optimizer = torch.optim.Adam(model.parameters(), lr=1e-3, weight_decay=1e-4)
rollouts = RolloutStorage(forward_steps, state_dim, action_dim)


def exp_func(x):
    # return 21.9995 * np.exp(-3988.6108 * x) + 0.09245
    # return 20.002 * np.exp(-14.066 * x) + 0.1
    return 12.922 * np.exp(-26.643 * x) + 0.1

def exp_func2(x):
    # return x * (-850000.0) + 180.0
    # return x * (-2666.667) + 143.333
    return x * (-17000.0) + 180.0

def cal_reward(qoe_throughput, delay, rtt):
    # delay += rtt
    if delay < 0.005:
        # qoe_delay = delay * (-50000.0) + 100.0
        # qoe_delay = delay * (-500.0) + 100.0
        qoe_delay = delay * (-1000.0) + 100.0
    elif delay < 0.01:
        qoe_delay = exp_func2(delay)
    else:
        qoe_delay = exp_func(delay)
    qoe = qoe_throughput * qoe_delay / 100.0
    return qoe

#def cal_reward(qoe_throughput, delay, rtt):
#    # if delay < 0.06:
#    #     qoe_delay = delay * (-83.3) + 100.0
#    # elif delay < 0.1:
#    #     qoe_delay = delay * (-2125.0) + 222.5
#    # else:
#    #     qoe_delay = 17.899 * np.exp(-5.871 * delay) + 0.050
#    if delay < 0.08:
#        qoe_delay = delay * (-62.50) + 100.0
#    elif delay < 0.15:
#        qoe_delay = delay * (-1214.286) + 192.143
#    else:
#        qoe_delay = 27.383 * np.exp(-6.761 * delay) + 0.068
#    qoe = qoe_throughput * qoe_delay / 100.0
#    return qoe


def get_state(last_state, cur_state, reward_delay, reward_throughput, rtt, mode, t):
    global reward_list, action, first_flag, score, last_reward, sum_reward, count_reward, action_log_probs, value

    reward = cal_reward(reward_throughput, reward_delay, rtt)  
    
    if mode == 'Training':
        if t < 200:
            score += reward
            if last_state.shape[0] != 0:
                rollouts.insert(t-window_size, cur_state, action, action_log_probs, value[0], reward)
            else:
                rollouts.initialize(cur_state)
            state = torch.FloatTensor(cur_state.reshape(1, window_size, -1)).to(device)
            value, action, action_log_probs = model.act(state, mode)

    if mode == 'Testing':
        if t >= 200:
            score += reward
            state = torch.FloatTensor(cur_state.reshape(1, window_size, -1)).to(device)
            value, action, action_log_probs = model.act(state, mode)
            
    print('Reward: ', reward)
    print('Action:', action)


def parse_data(msg):
    global last_drop, last_retran, throughput_list, delay_list
    low_queue_delay, high_queue_delay, avg_queue_delay, moving_queue_delay, low_queue_len, high_queue_len, avg_queue_len, moving_queue_len, throughput, rtt, spare, spare_count, dequeue, enqueue, num_retrans, num_drop, num_flows = struct.unpack(
        '11d6i', msg)
    retran = num_retrans - last_retran
    drop = num_drop - last_drop
    last_retran = num_retrans
    last_drop = num_drop
    throughput_list.append(throughput)
    delay_list.append(avg_queue_delay)
    rtt /= 1000000.0
    rtt -= avg_queue_delay
    print([avg_queue_delay/0.01, throughput / 1024, float(enqueue)/125000 / 1024, rtt/0.11])
    return [avg_queue_delay/0.01, throughput / 1024, float(enqueue)/125000 / 1024, rtt/0.11], [throughput, avg_queue_delay, drop], rtt


def run():
    global min_thre_list, max_thre_list, cur_state, last_state, throughput_list, delay_list, best_tag, score, first_flag, action, score_list_train, score_list_test, best_delay_train, best_delay_test, best_throughput_train, best_throughput_test, best_length_train, best_enqueue_train, best_enqueue_test, best_length_test, max_score_train, max_score_test, window_size
    t = -1
    score = 0.0
    capacity_file = './trace/link_capacity.txt'
    with open(capacity_file, 'r') as fb:  
        capacity_trace = fb.read().strip().split('\n')
    capacity_x = []
    capacity_list = []
    for row in capacity_trace:
        tmp = row.strip().split('\t')
        #tmp = row.strip().split(' ')
        # time1 = float(tmp[0])
        if (len(tmp) < 2):
            c = 0.0
        else:
            c = float(tmp[1])
        # capacity_x.append(time1)
        capacity_list.append(c/ 1000000)

    sleep_time = float(1.0)
    mode = 'Training'
    cur_state = np.array([])
    last_state = np.array([])
    while True:
        t += 1
        print(t)
        time1 = t * 100 + 99
        tcpSock, clientAddr = tcpServer.accept()
        start_time = time.time()
        msg = tcpSock.recv(112)
        # print (str(msg))
        if msg == bytes('SIMULATION END!', encoding='UTF-8'):
            print('okkkkkkkkkkkkkkkkkk')
            break
        
        state, reward, rtt = parse_data(msg)
        #state[1] = state[1] / action[0] / 10000.0
        state.append(capacity_list[time1] / 1024)

        if cur_state.shape[0] == 0:
            cur_state = np.array([state])
        else:
            cur_state = np.row_stack((cur_state, np.array(state)))
        if cur_state.shape[0] > window_size:
            cur_state = cur_state[1:]
        # print (state, reward)

        if t >= window_size:
            if reward[0] >= capacity_list[time1]:
                ratio_use = 100.0
            else:
                ratio_use = reward[0] / capacity_list[time1] * 100
            get_state(last_state, cur_state, reward[1], ratio_use, rtt, mode, t)
            print ('Action: ', action)
            last_state = cur_state

        # send action which is the function of the capacity
        if t >= window_size:
            if (mode == 'Training' and t < 200) or (mode == 'Testing' and t >= 200):
                if capacity_list[time1] == 0:
                    min_th = 0.0001
                else:
                    # min_th = pow(action[0], float(1.0) / capacity_list[t + 98] * 20.0) / 2
                    min_th = (float(action[0]) + 1.0) / 10000.0
                    print ('Min: ', min_th)
                res = struct.pack('5f', min_th, min_th, 1.0, sleep_time, 10.0)
                min_thre_list.append(min_th)
            else:
                res = struct.pack('5f', 0.0005, 0.0005, 1.0, sleep_time, 10.0)
        else:
            res = struct.pack('5f', 0.0005, 0.0005, 1.0, sleep_time, 10.0)

        tcpSock.send(res)
        end_time = time.time()
        # print('Using time:', end_time - start_time)
        if t == 199:
            mode = 'Testing'
            score_list_train.append(score)
            if score > max_score_train:
                max_score_train = score
                best_tag = True
            score = 0.0
    score_list_test.append(score)
    if score > max_score_test:
        max_score_test = score
        best_throughput_test = throughput_list
        best_delay_test = delay_list
        best_length_test = length_list
        best_enqueue_test = enqueue_list
    if best_tag:
        best_tag = False
        best_throughput_train = throughput_list
        best_delay_train = delay_list
        best_enqueue_train = enqueue_list
        best_length_train = length_list


def main():
    global epsilon, min_thre_list, average_reward, sum_reward, count_reward, reward_list, best_tag, first_flag, throughput_list, delay_list, length_list, enqueue_list, best_delay_train, best_delay_test, best_throughput_train, best_throughput_test, score_list_train, score_list_test
    for i_episode in range(1000):
        print('epoch: ', i_episode)
        throughput_list = []
        delay_list = []
        length_list = []
        enqueue_list = []
        # min_thre_list = []
        first_flag = False
        run()
        update_model()
        epsilon *= 0.99
        # average_reward = 0.9 * average_reward + 0.1 * sum_reward / count_reward
        # sum_reward = 0.0
        # count_reward = 0
        if i_episode % 1 == 0:
            snapshot = {
                'epoch': i_episode,
                'model': model.state_dict(),
                'optimizer': optimizer.state_dict(),
            }
            torch.save(snapshot, os.path.join('a2c_test', 'wirelesssepoch-{}.pth'.format(i_episode)))
            with open('score_train_new_dcnrtthop_a2c_wireless_decay_noise_1t_1d_mix.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(score_list_train)
            with open('score_test_new_dcnrtthop_a2c_wireless_decay_noise_1t_1d_mix.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(score_list_test)
            with open('throughput_train_new_dcnrtthop_a2c_wireless_decay_noise_1t_1d_mix.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(best_throughput_train)
            with open('delay_train_new_dcnrtthop_a2c_wireless_decay_noise_1t_1d_mix.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(best_delay_train)
            with open('throughput_test_new_dcnrtthop_a2c_wireless_decay_noise_1t_1d_mix.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(best_throughput_test)
            with open('delay_test_new_dcnrtthop_a2c_wireless_decay_noise_1t_1d_mix.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(best_delay_test)
            with open('enqueue_train_new_dcnrtthop_a2c_wireless_decay_noise_1t_1d_mix.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(best_enqueue_train)
            with open('length_train_new_dcnrtthop_a2c_wireless_decay_noise_1t_1d_mix.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(best_length_train)
            with open('enqueue_test_new_dcnrtthop_a2c_wireless_decay_noise_1t_1d_mix.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(best_enqueue_test)
            with open('length_test_new_dcnrtthop_a2c_wireless_decay_noise_1t_1d_mix.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(best_length_test)
            with open('min_thre_new_dcnrtthop_a2c_wireless_decay_noise_1t_1d_mix.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(min_thre_list)
            with open('new_throughput_a2c_wireless_decay_noise_1t_1d_mix.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(throughput_list)
            with open('new_delay_a2c_wireless_decay_noise_1t_1d_mix.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(delay_list)
            # with open('max_thre_new_dcnrtthop_1.csv', 'w') as fb:
            #     csvfile = csv.writer(fb)
            #     csvfile.writerow(max_thre_list)
    print(score_list_train)
    print(score_list_test)
    with open('score_train_new_dcnrtthop_a2c_wireless_decay_noise_1t_1d_mix.csv', 'w') as fb:
        csvfile = csv.writer(fb)
        csvfile.writerow(score_list_train)
    with open('score_test_new_dcnrtthop_a2c_wireless_decay_noise_1t_1d_mix.csv', 'w') as fb:
        csvfile = csv.writer(fb)
        csvfile.writerow(score_list_test)
    with open('throughput_train_new_dcnrtthop_a2c_wireless_decay_noise_1t_1d_mix.csv', 'w') as fb:
        csvfile = csv.writer(fb)
        csvfile.writerow(best_throughput_train)
    with open('delay_train_new_dcnrtthop_a2c_wireless_decay_noise_1t_1d_mix.csv', 'w') as fb:
        csvfile = csv.writer(fb)
        csvfile.writerow(best_delay_train)
    with open('throughput_test_new_dcnrtthop_a2c_wireless_decay_noise_1t_1d_mix.csv', 'w') as fb:
        csvfile = csv.writer(fb)
        csvfile.writerow(best_throughput_test)
    with open('delay_test_new_dcnrtthop_a2c_wireless_decay_noise_1t_1d_mix.csv', 'w') as fb:
        csvfile = csv.writer(fb)
        csvfile.writerow(best_delay_test)
    with open('enqueue_train_new_dcnrtthop_a2c_wireless_decay_noise_1t_1d_mix.csv', 'w') as fb:
        csvfile = csv.writer(fb)
        csvfile.writerow(best_enqueue_train)
    with open('length_train_new_dcnrtthop_a2c_wireless_decay_noise_1t_1d_mix.csv', 'w') as fb:
        csvfile = csv.writer(fb)
        csvfile.writerow(best_length_train)
    with open('enqueue_test_new_dcnrtthop_a2c_wireless_decay_noise_1t_1d_mix.csv', 'w') as fb:
        csvfile = csv.writer(fb)
        csvfile.writerow(best_enqueue_test)
    with open('length_test_new_dcnrtthop_a2c_wireless_decay_noise_1t_1d_mix.csv', 'w') as fb:
        csvfile = csv.writer(fb)
        csvfile.writerow(best_length_test)
    with open('min_thre_new_dcnrtthop_a2c_wireless_decay_noise_1t_1d_mix.csv', 'w') as fb:
        csvfile = csv.writer(fb)
        csvfile.writerow(min_thre_list)
    # with open('max_thre_new_dcnrtthop_1.csv', 'w') as fb:
    #     csvfile = csv.writer(fb)
    #     csvfile.writerow(max_thre_list)


if __name__ == '__main__':
    main()
