import matplotlib.pyplot as plt
import numpy as np
import os
import csv

csv_dir = '/results/csv/'
graph_dir = '/results/graphs/'

# Plot Type 1: Pinned Pages Over Time
policies = ['LRU', 'Client', 'Hot_Key', 'Random']
for i in range(1, 5):
    policy = policies[i - 1]
    name = os.getenv('PWD') + csv_dir + 'pinning' + str(i) + '.csv'
    print(name)
    pages_over_time = []
    labels = []
    num_pages = 0
    offset = 0
    num_evict = ''
    with open(name) as csv_file:
        csv_reader = csv.reader(csv_file, delimiter=',')
        line_count = 0
        cnt = 0
        for row in csv_reader:
           # print(row)
            if row[0] == "Heuristics":
                num_pages = int(row[3])
                offset = int(row[1])
                num_evict = row[2]
                for i in range(0, 91, offset):
                    labels.append(i)
                for j in range(0, num_pages):
                    pages_over_time.append([])
                # print(pages_over_time, " and " , labels)
            else:
                for i in range(0, num_pages):
                    # for j in range(0, len(labels), offset):
                    # print("Index: ", j, " Num_pages: ", i)
                    # print(row[1:len(row)])
                    if str(i) in row[1:len(row)]:
                        print("Unpin: ", str(i))
                        pages_over_time[i].append(0)
                    else:
                        pages_over_time[i].append(1)
                cnt += 1
        print(num_pages)
        width = 0.35       # the width of the bars: can also be len(x) sequence
        fig, ax = plt.subplots()
        if len(pages_over_time) == 0:
            print("Uh oh! No pages were monitored!")
            quit()
        # print(len(labels))
        print("Policy: ", pages_over_time)
        # print(len(pages_over_time[1]))
        ax.bar(labels, np.array(pages_over_time[0]), label='Page 0')
        last_pages = np.array(pages_over_time[0])

        for i in range(1, len(pages_over_time)):
            ax.bar(labels, pages_over_time[i], bottom=last_pages, label='Page ' + str(i))
            last_pages = last_pages + np.array(pages_over_time[i])
            # print(len(last_pages))
        
        ax.set_ylabel('# of Pinned Pages')
        ax.set_xlabel('Seconds')
        ax.set_title(policy + ' Pinned Pages Over Time')
        box = ax.get_position()
        ax.set_position([box.x0, box.y0, box.width * 0.8, box.height])
        ax.legend(loc='center left', bbox_to_anchor=(1, 0.5))
        fig_name = os.getenv('PWD') + graph_dir + policy + '_pin_over_time_' + str(offset) + '_' + num_evict + '.png'
        plt.savefig(fig_name)

# Plot Type 2: Number of pages pinned over time
y_axis_sub = []
y_axis_add = []
x = []
offset = 0
num_evict = ''
skip = 1
for i in range(1, 5):
    policy = policies[i - 1]
    name = os.getenv('PWD') + csv_dir +'perf' + str(i) + '.csv'
    print(name)
    num_sub = []
    num_add = []
    num_pages = 0
    with open(name) as csv_file:
        csv_reader = csv.reader(csv_file, delimiter=',')
        line_count = 0
        cnt = 0
        for row in csv_reader:
            if row[0] == "Heuristics" and skip:
                num_pages = int(row[3])
                offset = int(row[1])
                num_evict = row[2]
                for i in range(0, 91, offset):
                    x.append(i)
                # print(pages_over_time, " and " , labels)
            elif row[0] != "Heuristics":
                # for i in range(0, len(labels), offset):
                num_sub.append(int(row[2]))
                num_add.append(int(row[1]))
            skip = 0
    y_axis_sub.append(num_sub)
    y_axis_add.append(num_add)
print(x)
print(len(x))
print(y_axis_add[3])
print(len(y_axis_sub[0]))
plt.clf()
plt.plot(x, y_axis_add[0], label="LRU")
plt.plot(x, y_axis_add[3], label="Rand")
plt.plot(x, y_axis_add[1], label="Client")
plt.plot(x, y_axis_add[2], label="Hot Keys")
plt.xlabel("Seconds")
plt.ylabel("Number of Newly Pinned Pages")
plt.title("Number of Newly Pinned Pages Over Time")
plt.legend()
fig_name = os.getenv('PWD') + graph_dir + 'all_num_pinned_' + str(offset) + '_' + num_evict + '.png'
plt.savefig(fig_name)

plt.clf()
plt.plot(x, y_axis_sub[0], label="LRU")
plt.plot(x, y_axis_sub[3], label="Rand")
plt.plot(x, y_axis_sub[1], label="Client")
plt.plot(x, y_axis_sub[2], label="Hot Keys")
plt.xlabel("Seconds")
plt.ylabel("Number of Recently Unpinned Pages")
plt.title("Number of Recently Unpinned Pages Over Time")
plt.legend()
fig_name = os.getenv('PWD') + graph_dir + 'all_num_unpinned_' + str(offset) + '_' + num_evict + '.png'
plt.savefig(fig_name)
