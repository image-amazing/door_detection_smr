#!/usr/bin/env python

from __future__ import print_function

import roslib
roslib.load_manifest('door_detection')
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.pyplot import plot, draw, show
import sys
import rospy
import pcl_ros
import cv2
from std_msgs.msg import String
from sensor_msgs.msg import Image, PointCloud2, PointField
import sensor_msgs.point_cloud2 as pc2
from cv_bridge import CvBridge, CvBridgeError
from sklearn.cluster import KMeans
import message_filters
from sensor_msgs.msg import Image, CameraInfo


class image_converter:

  def __init__(self):
    rospy.init_node('image_converter', anonymous=True)
    self.image_pub = rospy.Publisher("my_image",Image, queue_size="10")
    self.bridge = CvBridge()
    #self.pub_sub = rospy.Subscriber("camera/depth_registered/points", PointCloud2, self.callback_kinect)
    self.image_sub = rospy.Subscriber("camera/rgb/image_color",Image,self.callback_image)
   

  def callback_pcl(self,data):
    try:
      cv_pcl = self.bridge.imgmsg_to_cv2(data, "mono16")
    except CvBridgeError as e:
      print(e)

    cv2.imshow("PointCloud window", cv_pcl)
    cv2.waitKey(3)

  def read_depth(width, height, data) :
    # read function
    if (height >= data.height) or (width >= data.width) :
        return -1
    data_out = pc2.read_points(data, field_names=None, skip_nans=False, uvs=[[width, height]]) 
    int_data = next(data_out)
    rospy.loginfo("int_data " + str(int_data))
    return int_data

  def point_2_world(data):
    pass

  def callback_image(self,data):
    try:
      cv_image = self.bridge.imgmsg_to_cv2(data, "bgr8")
    except CvBridgeError as e:
      print(e)

    # K_mean color clustering
    Z = cv_image.reshape((-1,3))

    # convert to np.float32
    Z = np.float32(Z)

    # define criteria, number of clusters(K) and apply kmeans()
    K = 2
    criteria = (3, 10, 1.0)
    ret,label,center=cv2.kmeans(Z,K,criteria,1,cv2.KMEANS_RANDOM_CENTERS)
        
    # Now convert back into uint8, and make original image
    center = np.uint8(center)
    res = center[label.flatten()]
    res2 = res.reshape((cv_image.shape))
    
    res3=cv2.cvtColor(res2,cv2.COLOR_RGB2GRAY)
    (ret,thresh) = cv2.threshold(res3,128,255,cv2.THRESH_BINARY)

    ## CONTOURS
    contours,hierarchy = cv2.findContours(thresh, 1,2)
    for cnt in contours: 
        if cnt.shape[0] < 40  or cnt.shape[0] > 100:
		continue 
	rect = cv2.minAreaRect(cnt)
        box = cv2.cv.BoxPoints(rect)
        box = np.int0(box)
        cv2.drawContours(cv_image,[box],0,(0,0,255),2)


    cv2.imshow("Image window", cv_image)
    #cv2.imshow("PointCloud window", cv_pcl)
    cv2.waitKey(3)

    try:
      self.image_pub.publish(self.bridge.cv2_to_imgmsg(cv_image, "bgr8"))
    except CvBridgeError as e:
      print(e)

if __name__ == '__main__':
  image_converter()
  try:
    rospy.spin()
  except KeyboardInterrupt:
    print("Shutting down")
  cv2.destroyAllWindows()


