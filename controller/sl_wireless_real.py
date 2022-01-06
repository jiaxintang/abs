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
last_action = 0
last_size = 0
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
batch_size = 128
log_interval = 10
capacity = 500
epoch = 100
epsilon = 0.1
forward_steps = 5000 - window_size
ppo_clip_param = 0.1
entropy_coef = 0.1
loss_fun = nn.CrossEntropyLoss()
total_update = []
queue_quality = 0
true_latest = []


def weights_init_normal(m):
    classname = m.__class__.__name__
    if isinstance(m, nn.Linear):
        init.kaiming_normal_(m.weight, mode='fan_in')
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


class RNNPreditor(nn.Module):
    def __init__(self, state_dim, action_dim):
        super(RNNPreditor, self).__init__()
        hidden_dim = 5
        self.lstm = nn.LSTM(input_size=state_dim, hidden_size=hidden_dim, bidirectional=False, batch_first=True)
        self.classifier = nn.Sequential(
                            nn.Linear(hidden_dim, 50), nn.ReLU(),
                            nn.Linear(50, 40), nn.ReLU(),
                            nn.Linear(40, action_dim)
                          )

    def forward(self, inputs):
        output, (h_n, c_n) = self.lstm(inputs)
        output = output[:, -1, :]
        output = self.classifier(output)
        output = F.softmax(output)
        return output
    

def update_model():
    inputs = rollouts.states[:-1]
    trues = rollouts.true.squeeze().long()
    batch_size = 32
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
model = RNNPreditor(state_dim, action_dim)
model.apply(weights_init_normal)
parameters = filter(lambda p: p.requires_grad, model.parameters())
optimizer = torch.optim.Adam(model.parameters(), lr=1e-3)
rollouts = RolloutStorage(forward_steps, action_dim)

def cal_reward(qoe_throughput, delay, rtt):
    if delay < 0.08:
        qoe_delay = delay * (-62.50) + 100.0
    elif delay < 0.15:
        qoe_delay = delay * (-1214.286) + 192.143
    else:
        qoe_delay = 27.383 * np.exp(-6.761 * delay) + 0.068
    qoe = qoe_throughput * qoe_delay / 100.0
    return qoe


def get_state(last_state, cur_state, reward_delay, reward_throughput, rtt, mode, t):
    global reward_list, action, first_flag, score, last_reward, sum_reward, count_reward, action_log_probs, value, standing

    reward = cal_reward(reward_throughput, reward_delay, rtt)  
    
    if mode == 'Training':
        if t < 5000:
            score += reward
            if last_state.shape[0] != 0:
                rollouts.insert(t-window_size, cur_state, standing)
            else:
                rollouts.initialize(cur_state)
            state = torch.FloatTensor(cur_state.reshape(1, window_size, -1)).to(device)
            action = model.forward(state)

    if mode == 'Testing':
        if t >= 5000:
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
    print([avg_queue_delay/0.001, throughput / 100, float(enqueue)/125000 / 100, rtt/0.11])
    return [avg_queue_delay/0.001, throughput / 100, float(enqueue)/125000 / 100, rtt/0.11], [throughput, avg_queue_delay, drop], rtt, low_queue_len, spare / 100.0, spare_count


def run():
    global min_thre_list, max_thre_list, cur_state, last_state, throughput_list, delay_list, best_tag, score, first_flag, action, standing, score_list_train, score_list_test, best_delay_train, best_delay_test, best_throughput_train, best_throughput_test, best_length_train, best_enqueue_train, best_enqueue_test, best_length_test, max_score_train, max_score_test, window_size, last_action, last_size, queue_quality, true_latest
    t = -1
    score = 0.0
    capacity_file = './trace/Verizon-LTE-driving.down.tr'
    with open(capacity_file, 'r') as fb:  
        capacity_trace = fb.read().strip().split('\n')
    capacity_x = []
    capacity_list = []
    for row in capacity_trace[1:]:
        tmp = row.strip().split('\t')
        if (len(tmp) < 2):
            c = 0.0
        else:
            c = float(tmp[1])
        capacity_list.append(c)

    sleep_time = float(0.1)
    mode = 'Training'
    cur_state = np.array([])
    last_state = np.array([])
    true_latest = []
    while True:
        t += 1
        print(t)
        time1 = t * 10 + 9
        tcpSock, clientAddr = tcpServer.accept()
        start_time = time.time()
        msg = tcpSock.recv(112)
        if msg == bytes('SIMULATION END!', encoding='UTF-8'):
            print('SIMULATION END!')
            break
        
        state, reward, rtt, standing, spare, spare_count = parse_data(msg)
        if standing == 0:
            queue_quality += 1
        print ('Standing: ', standing)
        if standing > 0:
            true_latest.append(standing)
            standing = standing + last_action - 0
        else:
            enlarge = sleep_time * spare * capacity_list[time1] * 1000000 / 1500.0 / 8.0
            if spare_count > 0:
                enlarge /= float(spare_count)
            else:
                enlarge = 0.0
            print ('Enlarge:', enlarge)
            standing = last_action - enlarge - 0
            true_latest.append(-enlarge)
        print (standing)
        standing = int(standing / 1) + 10
        if standing < 0:
            standing = 0
        if standing > 20:
            standing = 20
        state.append(capacity_list[time1] / 10.0)

        if cur_state.shape[0] == 0:
            cur_state = np.array([state])
        else:
            cur_state = np.row_stack((cur_state, np.array(state)))
        if cur_state.shape[0] > window_size:
            cur_state = cur_state[1:]
        # print (state, reward)

        if t >= window_size:
            print ("Throughput / bw", reward[0], capacity_list[time1])
            if reward[0] >= capacity_list[time1]:
                ratio_use = 100.0
            else:
                ratio_use = reward[0] / capacity_list[time1] * 100
            with torch.no_grad():
                get_state(last_state, cur_state, reward[1], ratio_use, rtt, mode, t)
            print ('Action: ', action)
            last_state = cur_state

        # send action which is the function of the capacity
        if t >= window_size:
            if (mode == 'Training' and t < 5000) or (mode == 'Testing' and t >= 5000):
                
                dist = Categorical(action)
                predict_standing = dist.sample()
                print (predict_standing)
                tmp = 1 * (predict_standing - 10)
                last_action = tmp
                if last_size <= tmp: 
                    min_th = 1
                else:
                    min_th = last_size - tmp
                last_size = min_th
                print ('Min: ', predict_standing, standing)
                res = struct.pack('5f', min_th / 10000.0, min_th / 10000.0, 1.0, sleep_time, 10.0)
                min_thre_list.append(min_th)
            else:
                res = struct.pack('5f', 0.001, 0.001, 1.0, sleep_time, 10.0)
                last_size = 10
        else:
            res = struct.pack('5f', 0.001, 0.001, 1.0, sleep_time, 10.0)
            last_size = 10

        tcpSock.send(res)
        end_time = time.time()
        if t == 4999:
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
    print ('Queue quality: ', queue_quality)
    print (true_latest)


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
            torch.save(snapshot, os.path.join('sl_test_new', 'wirelessepoch-{}.pth'.format(i_episode)))
            with open('score_train_new_dcnrtthop_a2c_wireless_real_ver_111_mix.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(score_list_train)
            with open('score_test_new_dcnrtthop_a2c_wireless_real_ver_111_mix.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(score_list_test)
            with open('throughput_train_new_dcnrtthop_a2c_wireless_real_ver_111_mix.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(best_throughput_train)
            with open('delay_train_new_dcnrtthop_a2c_wireless_real_ver_111_mix.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(best_delay_train)
            with open('throughput_test_new_dcnrtthop_a2c_wireless_real_ver_111_mix.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(best_throughput_test)
            with open('delay_test_new_dcnrtthop_a2c_wireless_real_ver_111_mix.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(best_delay_test)
            with open('enqueue_train_new_dcnrtthop_a2c_wireless_real_ver_111_mix.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(best_enqueue_train)
            with open('length_train_new_dcnrtthop_a2c_wireless_real_ver_111_mix.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(best_length_train)
            with open('enqueue_test_new_dcnrtthop_a2c_wireless_real_ver_111_mix.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(best_enqueue_test)
            with open('length_test_new_dcnrtthop_a2c_wireless_real_ver_111_mix.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(best_length_test)
            with open('min_thre_new_dcnrtthop_a2c_wireless_real_ver_111_mix.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(min_thre_list)
            with open('new_throughput_a2c_wireless_real_ver_111_mix.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(throughput_list)
            with open('new_delay_a2c_wireless_real_ver_111_mix.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(delay_list)
            with open('loss_wireless_real_ver_111_mix.csv', 'w') as fb:
                csvfile = csv.writer(fb)
                csvfile.writerow(total_update)
    print(score_list_train)
    print(score_list_test)


if __name__ == '__main__':
    main()
