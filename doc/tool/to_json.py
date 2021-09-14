import os
import sys
import re
import json
import matplotlib.pyplot as plt

option_regex = re.compile(r"[0-9]+")
option_start = re.compile(r"mvcc bench")
outcome_regex = re.compile(r"[0-9\.]+")
outcome_start = re.compile(r"[#]+ total [#]+")
options = ["#threads", "Initial Size", "Buckets", "Duration",
           "Update Rate", "Range"]
outcome_ops = ["Set Size", "Duration", "#ops", "#read ops", "#update ops"]


def dump_to_file(file_name, final_content):
    with open(file_name + ".json", "w") as j:
        j.write(json.dumps(final_content, indent=4, sort_keys=False))


def count_ops(header, content):
    counted = {"read": 0, "write": 0}
    counted["#threads"] = int(header[options[0]])
    for c in content:
        counted["read"] += float(c[outcome_ops[3]])
        counted["write"] += float(c[outcome_ops[4]])
    return counted


def data_to_x_y(json_data):
    x_y_vals = {}
    sec = {}
    for raw in json_data:
        counted = count_ops(raw["header"], raw["content"])
        x_y_vals[counted["#threads"]] = counted["read"] + counted["write"]
        sec[counted["#threads"]] = int(raw["header"][options[3]]) * len(raw["content"])

    for key in x_y_vals.keys():
        x_y_vals[key] /= sec[key] / 1000

    return x_y_vals


def option_parser(f):
    parsed_options = {}

    for opt in options:
        parsed_options[opt] = option_regex.findall(f.readline())[0]

    return parsed_options


def outcome_parser(f):
    parsed_outcomes = {}

    for ops in outcome_ops:
        parsed_outcomes[ops] = outcome_regex.findall(f.readline())[0]

    return parsed_outcomes


def qemu_output_to_dic(file_name):
    header = ""
    content = []
    final_content = []
    with open(file_name, "r") as f:
        while True:
            line = f.readline()
            if option_start.findall(line):
                new_header = option_parser(f)
                if new_header != header:
                    if content:
                        final_content.append({
                                "header": header.copy(),
                                "content": content.copy(),
                            })
                    header = new_header
                    content = []
            elif outcome_start.findall(line):
                item = outcome_parser(f)
                content.append(item)
            elif not line:
                break
        final_content.append({
                "header": header.copy(),
                "content": content.copy(),
            })
    return final_content


if __name__ == '__main__':
    if os.system("dos2unix -f -ascii " + sys.argv[1]) != 0: exit()
    parsed_qemu = qemu_output_to_dic(sys.argv[1])

    x_y_vals = data_to_x_y(parsed_qemu)
    print(json.dumps(x_y_vals, indent=4, sort_keys=False))
    plt.plot(x_y_vals.keys(), x_y_vals.values(), "-o")
    plt.grid()
    plt.ylabel("total ops per sec")
    plt.xlabel("#threads")
    plt.show()

    dump_to_file(sys.argv[1], parsed_qemu)
