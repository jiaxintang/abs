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
action_dim = 21
learning_rate = 0.001
update_iteration = 5
tau = 0.01
action_bias = 0.0015
gamma = 0.9
batch_size = 16 
log_interval = 10
capacity = 500
epoch = 100
epsilon = 0.1
forward_steps = 200 - window_size
ppo_clip_param = 0.1
entropy_coef = 0.1
last_size = 0
last_action = 0
loss_fun = nn.CrossEntropyLoss()
total_update = []

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


class RolloutStorage(object):
    def __init__(self, num_steps, action_dim):
        self.true = torch.zeros(num_steps, 1)
        self.states = torch.zeros(num_steps + 1, window_size, state_dim)
        
        self.step = 0
        self.num_steps = num_steps

    def insert(self, step, state, true):
        true = torch.FloatTensor([true])
        state = torch.FloatTensor(state)
        self.states[step + 1].copy_(state)
        self.true[step].copy_(true)

    def initialize(self, state):
        state = torch.FloatTensor(state)
        self.true = torch.zeros(self.num_steps, 1)
        self.states = torch.zeros(self.num_steps + 1, window_size, state_dim)
        self.states[0].copy_(state)

    def get_batch(self, batch_ix):
        return self.states[batch_ix], self.returns[batch_ix], self.actions[batch_ix], self.action_log_probs[batch_ix], self.value_preds[batch_ix]

class RNNPredictor(nn.Module):
    def __init__(self, state_dim, action_dim):
        super(RNNPredictor, self).__init__()
        hidden_dim = 5 
        self.lstm = nn.LSTM(input_size=state_dim, hidden_size=hidden_dim, bidirectional=False, batch_first=True)
        self.classifier = nn.Sequential(
                        nn.Linear(hidden_dim, 50), nn.ReLU(),
                        nn.Linear(50, 40), nn.ReLU(),
                        nn.Linear(40, action_dim)
                        )

    def forward(self, x):
        output, (h_n, c_n) = self.lstm(x)
        output = output[:, -1, :]
        output = self.classifier(output)
        output = F.softmax(output)
        return output


def update_model():
    inputs = rollouts.states[:-1]
    trues = rollouts.true.squeeze().long()
    batch_size = 10
    batch_num = (forward_steps) / batch_size
    batch_num = int(batch_num)
    temp = np.array(range(forward_steps))
    np.random.shuffle(temp)
    for i in range(batch_num):
        batch_ix = []
        for j in range(batch_size * i, batch_size * (i + 1)):
            batch_ix.append(temp[j])
        input_batch = inputs[batch_ix]
        true_batch = trues[batch_ix]
        predict = model.forward(input_batch)
        total_loss = loss_fun(predict, true_batch)
        total_update.append(total_loss)
        optimizer.zero_grad()
        total_loss.backward()
        optimizer.step()


# ---------------------------Loading Models----------------------------#
model = RNNPredictor(state_dim, action_dim)
model.apply(weights_init_normal)
parameters = filter(lambda p: p.requires_grad, model.parameters())
optimizer = torch.optim.Adam(model.parameters(), lr=1e-3)
rollouts = RolloutStorage(forward_steps, action_dim)


def exp_func(x):
    return 21.9995 * np.exp(-3988.6108 * x) + 0.09245

def exp_func2(x):
    return x * (-850000.0) + 180.0

def cal_reward(qoe_throughput, delay, rtt):
    # delay += rtt
    if delay < 0.0001:
        qoe_delay = delay * (-50000.0) + 100.0
    elif delay < 0.0002:
        qoe_delay = exp_func2(delay)
    else:
        qoe_delay = exp_func(delay)
    qoe = qoe_throughput * qoe_delay / 100.0
    return qoe


def get_state(last_state, cur_state, reward_delay, reward_throughput, rtt, mode, t):
    global reward_list, action, first_flag, score, last_reward, sum_reward, count_reward, action_log_probs, value, standing

    reward = cal_reward(reward_throughput, reward_delay, rtt)  
    
    if mode == 'Training':
        if t < 200:
            score += reward
            if last_state.shape[0] != 0:
                rollouts.insert(t-window_size, cur_state, standing)
            else:
                rollouts.initialize(cur_state)
            state = torch.FloatTensor(cur_state.reshape(1, window_size, -1)).to(device)
            action = model.forward(state)

    if mode == 'Testing':
        if t >= 200:
            score += reward
            state = torch.FloatTensor(cur_state.reshape(1, window_size, -1)).to(device)
            action = model.forward(state)
            
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
    print([avg_queue_delay/0.0001, (throughput - 900) / (1024 - 900), (float(enqueue)/125000 - 900) / (1024 - 900), rtt/0.0005, spare, spare_count])
    return [avg_queue_delay/0.0001, (throughput - 900) / (1024 - 900), (float(enqueue)/125000 - 900) / (1024 - 900) , rtt/0.0005], [throughput, avg_queue_delay, drop], rtt, low_queue_len, spare / 100.0, spare_count


def run():
    global min_thre_list, max_thre_list, cur_state, last_state, throughput_list, delay_list, best_tag, score, first_flag, action, score_list_train, score_list_test, best_delay_train, best_delay_test, best_throughput_train, best_throughput_test, best_length_train, best_enqueue_train, best_enqueue_test, best_length_test, max_score_train, max_score_test, window_size, last_size, last_action, standing
    t = -1
    score = 0.0
    capacity_list = np.ones(10000)
    capacity_list = capacity_list * 1000.0

    sleep_time = float(1.0)
    mode = 'Training'
    cur_state = np.array([])
    last_state = np.array([])
    while True:
        t += 1
        print(t)
        tcpSock, clientAddr = tcpServer.accept()
        start_time = time.time()
        msg = tcpSock.recv(112)
        # print (str(msg))
        if msg == bytes('SIMULATION END!', encoding='UTF-8'):
            print('SIMULATION END!')
            break

        state, reward, rtt, standing, spare, spare_count = parse_data(msg)
        state.append(capacity_list[t + 98]/1000.0)
        
        print ('Standing: ', standing)
        if standing > 0:
            standing = standing + last_action - 0
        else:
            enlarge = sleep_time * spare * capacity_list[t + 98] * 1000 / 1500.0 / 8.0
            if spare_count > 0:
                enlarge /= float(spare_count)
            else:
                enlarge = 0.0
            print ('Enlarge:', enlarge)
            standing = last_action - enlarge - 0
        print (standing)
        standing = int(standing / 1) + 10
        if standing < 0:
            standing = 0
        if standing > 20:
            standing = 20

        if cur_state.shape[0] == 0:
            cur_state = np.array([state])
        else:
            cur_state = np.row_stack((cur_state, np.array(state)))
        if cur_state.shape[0] > window_size:
            cur_state = cur_state[1:]
        # print (state, reward)

        if t >= window_size:
            if reward[0] >= capacity_list[t+98]:
                ratio_use = 100.0
            else:
                ratio_use = reward[0] / capacity_list[t+98] * 100.0
            with torch.no_grad():
                get_state(last_state, cur_state, reward[1], ratio_use, rtt, mode, t)
            print ('Action: ', action)
            last_state = cur_state

        # send action which is the function of the capacity
        if t >= window_size:
            if (mode == 'Training' and t < 200) or (mode == 'Testing' and t >= 200):
                dist = Categorical(action)
                predict_standing = dist.sample()
                print (predict_standing)
                tmp = 1 * (predict_standing - 10)
                last_action = tmp
                if last_size <= tmp: 
                    min_th = 1
                elif last_size - tmp > 10000.0:
                    min_th = 10000.0
                else:
                    min_th = last_size - tmp
                last_size = min_th
                print ('Min: ', predict_standing, standing)
                res = struct.pack('5f', min_th / 10000.0, min_th / 10000.0, 1.0, sleep_time, 10.0)
                min_thre_list.append(min_th)
            else:
                res = struct.pack('5f', 0.0005, 0.0005, 1.0, sleep_time, 10.0)
                last_size = 5
        else:
            res = struct.pack('5f', 0.0005, 0.0005, 1.0, sleep_time, 10.0)
            last_size = 5

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
    for i_episode in range(300):
        print('epoch: ', i_episode)
        throughput_list = []
        delay_list = []
        length_list = []
        enqueue_list = []
        first_flag = False
        run()
        update_model()
        epsilon *= 0.99
        if i_episode % 1 == 0:
            snapshot = {
                'epoch': i_episode,
                'model': model.state_dict(),
                'optimizer': optimizer.state_dict(),
            }
            torch.save(snapshot, os.path.join('dcn_sl_test', 'epoch01-{}.pth'.format(i_episode)))
            with open('score_train_new_dcnrtthop_a2c_rttlarge_sl_mix.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(score_list_train)
            with open('score_test_new_dcnrtthop_a2c_rttlarge_sl_mix.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(score_list_test)
            with open('throughput_train_new_dcnrtthop_a2c_rttlarge_sl_mix.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(best_throughput_train)
            with open('delay_train_new_dcnrtthop_a2c_rttlarge_sl_mix.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(best_delay_train)
            with open('throughput_test_new_dcnrtthop_a2c_rttlarge_sl_mix.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(best_throughput_test)
            with open('delay_test_new_dcnrtthop_a2c_rttlarge_sl_mix.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(best_delay_test)
            with open('enqueue_train_new_dcnrtthop_a2c_rttlarge_sl_mix.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(best_enqueue_train)
            with open('length_train_new_dcnrtthop_a2c_rttlarge_sl_mix.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(best_length_train)
            with open('enqueue_test_new_dcnrtthop_a2c_rttlarge_sl_mix.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(best_enqueue_test)
            with open('length_test_new_dcnrtthop_a2c_rttlarge_sl_mix.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(best_length_test)
            with open('min_thre_new_dcnrtthop_a2c_rttlarge_sl_mix.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(min_thre_list)
            with open('new_throughput_a2c_rttlarge_sl_bbr_long.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(throughput_list)
            with open('new_delay_a2c_rttlarge_sl_bbr_long.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(delay_list)
            with open('loss_wireless_rttlarge_sl_2.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(total_update)
    print(score_list_train)
    print(score_list_test)
    

if __name__ == '__main__':
    main()
