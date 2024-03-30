# imports
from configs import config
from configs.detection import detect_people
from scipy.spatial import distance as dist
import numpy as np
import argparse
# import imutils
import cv2
import os


class BoundingBoxSmoother:
    def __init__(self, window_size=5):
        self.window_size = window_size
        self.history = []

    def update(self, bbox):
        self.history.append(bbox)
        if len(self.history) > self.window_size:
            self.history.pop(0)

        # Calculate the average bounding box
        avg_bbox = [sum(coords) / len(self.history)
                    for coords in zip(*self.history)]
        return avg_bbox


class SizeAverageFilter:
    def __init__(self, window_size=5):
        self.window_size = window_size
        self.history_width = []
        self.history_height = []

    def update(self, bbox):
        width, height = bbox

        # Update history
        self.history_width.append(width)
        self.history_height.append(height)

        # Ensure history does not exceed window size
        if len(self.history_width) > self.window_size:
            self.history_width.pop(0)
            self.history_height.pop(0)

        # Calculate the average size
        avg_width = sum(self.history_width) / len(self.history_width)
        avg_height = sum(self.history_height) / len(self.history_height)

        return avg_width, avg_height


# construct the argument parser and parse the arguments
ap = argparse.ArgumentParser()
ap.add_argument("-i", "--input", type=str, default="/home/oskar/Documents/Master Thesis/Experiments/Test for Video Player/video.avi",
                help="path to (optional) input video file")
ap.add_argument("-o", "--output", type=str, default="output.mp4",
                help="path to (optional) output video file")
ap.add_argument("-d", "--display", type=int, default=1,
                help="whether or not output frame should be displayed")
args = vars(ap.parse_args())

# load the COCO class labels the YOLO model was trained on
labelsPath = os.path.sep.join([config.MODEL_PATH, "coco.names"])
LABELS = open(labelsPath).read().strip().split("\n")

# derive the paths to the YOLO weights and model configuration
weightsPath = os.path.sep.join(
    [config.MODEL_PATH, "yolov4.weights"])
configPath = os.path.sep.join(
    [config.MODEL_PATH, "yolov4.cfg"])

# load the YOLO object detector trained on COCO dataset (80 classes)
print("[INFO] loading YOLO from disk...")
net = cv2.dnn.readNetFromDarknet(configPath, weightsPath)

# check if GPU is to be used or not
if config.USE_GPU:
    # set CUDA s the preferable backend and target
    print("[INFO] setting preferable backend and target to CUDA...")
    net.setPreferableBackend(cv2.dnn.DNN_BACKEND_CUDA)
    net.setPreferableTarget(cv2.dnn.DNN_TARGET_CUDA)

# determine only the "output" layer names that we need from YOLO
ln = net.getLayerNames()
ln = [ln[i - 1] for i in net.getUnconnectedOutLayers()]

# initialize the video stream and pointer to output video file
print("[INFO] accessing video stream...")
# open input video if available else webcam stream
vs = cv2.VideoCapture(args["input"] if args["input"] else 0)
# vs = cv2.VideoCapture("test.mp4")
writer = None

smoother = BoundingBoxSmoother(window_size=5)
size_filter = SizeAverageFilter(window_size=5)
# loop over the frames from the video stream
while True:
    # read the next frame from the input video
    (grabbed, frame) = vs.read()

    # if the frame was not grabbed, then that's the end fo the stream
    if not grabbed:
        break

    # resize the frame and then detect people (only people) in it
    frame = cv2.resize(frame, (640, 640))
    results = detect_people(frame, net, ln, size_filter,
                            personIdx=LABELS.index("person"))

    # initialize the set of indexes that violate the minimum social distance
    violate = set()
    old_results = []

    # ensure there are at least two people detections (required in order to compute the
    # the pairwise distance maps)
    if len(results) >= 2:
        # extract all centroids from the results and compute the Euclidean distances
        # between all pairs of the centroids
        centroids = np.array([r[2] for r in results])
        # left_corner = np.array([ (r[1][0], r[1][3]) for r in results])
        # right_corner = np.array([ (r[1][2], r[1][3]) for r in results])

        # normalized_centroids = tuple(rc / r[1][2] for rccentroids.
        # D = dist.cdist(left_corner, right_corner, metric="euclidean")

        # loop over the upper triangular of the distance matrix
        for i in range(0, centroids.shape[0]):
            # for j in range(i+1, D.shape[1]):
            for j in range(0, centroids.shape[0]):
                # check to see if the distance between any two centroid pairs is less
                # than the configured number of pixels
                if i < j:
                    dist = np.sqrt(pow(centroids[i][0]-centroids[j][0], 2) + pow(
                        centroids[i][1]-centroids[j][1], 2) + pow(centroids[i][2]-centroids[j][2], 2))
                    if (dist < config.MIN_DISTANCE):
                        # update the violation set with the indexes of the centroid pairs
                        violate.add(i)
                        violate.add(j)

    old_results = results
    # loop over the results
    for (i, (prob, bbox, centroid)) in enumerate(results):
        # extract teh bounding box and centroid coordinates, then initialize the color of the annotation
        (startX, startY, endX, endY) = bbox
        (cX, cY, distance) = centroid
        color = (0, 255, 0)

        # if the index pair exists within the violation set, then update the color
        if i in violate:
            color = (0, 0, 255)

        # print("Distance(cm):{dist}\n".format(dist=distance))
        text = "Depth: {} cm".format(round(distance))
        cv2.putText(frame, text, (startX + 10, startY + 100),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 2)
        # draw (1) a bounding box around the person and (2) the centroid coordinates of the person
        cv2.rectangle(frame, (startX, startY), (endX, endY), color, 2)
        # cv2.circle(frame, (cX, cY), 5, color, 1)

    # draw the total number of social distancing violations on the output frame
    text = "Social Distancing Violations: {}".format(len(violate))
    cv2.putText(frame, text, (10, frame.shape[0] - 25),
                cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 0, 255), 3)

    # check to see if the output frame should be displayed to the screen
    if args["display"] > 0:
        # show the output frame
        cv2.imshow("Output", frame)
        key = cv2.waitKey(1) & 0xFF

        # if the 'q' key is pressed, break from the loop
        if key == ord("q"):
            break

    # if an output video file path has been supplied and the video writer ahs not been
    # initialized, do so now
    # if args["output"] != "" and writer is None:
    #     # initialize the video writer
    #     fourcc = cv2.VideoWriter_fourcc(*"MJPG")
    #     writer = cv2.VideoWriter(
    #         args["output"], fourcc, 25, (frame.shape[1], frame.shape[0]), True)

    # # if the video writer is not None, write the frame to the output video file
    # if writer is not None:
    #     print("[INFO] writing stream to output")
    #     writer.write(frame)
