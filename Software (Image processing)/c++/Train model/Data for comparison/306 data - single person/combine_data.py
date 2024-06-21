import os
import re

def append(folder_path):
    common_file_path = os.path.join(folder_path, 'pixel_to_real_to_bb_mapping_entire.txt')
    matching_files = []

    for filename in os.listdir(folder_path):
        if re.match(r'pixel_to_real_to_bb_mapping_\d+\.txt', filename):
            matching_files.append(filename)

    matching_files.sort()

    with open(common_file_path, 'w') as common_file:
        for filename in matching_files:
            file_path = os.path.join(folder_path, filename)
            with open(file_path, 'r') as file:
                for line in file:
                    common_file.write(line)

folder_path = '/home/oskar/Documents/Master Thesis/Software (Image processing)/c++/Train model/306 data'
append(folder_path)