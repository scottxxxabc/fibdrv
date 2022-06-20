#!/usr/bin/env python3

import glob

filenames = glob.glob('time/*.time')

arr1 = []
arr2 = []
arr3 = []

for file in filenames:
    with open(file, 'r') as f:
        datas = f.readlines()
    data1 = []
    data2 = []
    data3 = []
    for data in datas:
        s = data.split();
        data1.append(int(s[1]))
        data2.append(int(s[2]))
        data3.append(int(s[3]))
    arr1.append(data1)
    arr2.append(data2)
    arr3.append(data3)

with open('time/time', 'w') as f:
    for i in range(len(arr1[0])):
        out1 = [arr1[j][i] for j in range(len(arr1))]
        out2 = [arr2[j][i] for j in range(len(arr2))]
        out3 = [arr3[j][i] for j in range(len(arr3))]
        #f.write('%i %f %f %f\n' % (i, sum(out1)/len(out1), sum(out2)/len(out2), sum(out3)/len(out3)))
        f.write('%i %f %f\n' % (i, sum(out2)/len(out2), sum(out3)/len(out3)))
