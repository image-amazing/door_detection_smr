#include <ros/ros.h>
#include <ros/callback_queue.h>
#include <sensor_msgs/PointCloud2.h>
#include <message_filters/subscriber.h>
#include <message_filters/synchronizer.h>
#include <message_filters/time_synchronizer.h>
#include <message_filters/sync_policies/approximate_time.h>
#include <boost/bind.hpp>
// PCL specific includes
#include <pcl_conversions/pcl_conversions.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/PCLPointCloud2.h>
#include <pcl/conversions.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/visualization/cloud_viewer.h>
#include <pcl/console/parse.h>
#include <pcl/common/transforms.h>
#include <pcl/filters/passthrough.h>
#include <pcl/sample_consensus/ransac.h>
#include <pcl/sample_consensus/method_types.h>
#include <pcl/sample_consensus/model_types.h>
#include <pcl/sample_consensus/sac_model_normal_plane.h>
#include <pcl/sample_consensus/sac_model_plane.h>
#include <pcl/segmentation/sac_segmentation.h>
#include <pcl/ModelCoefficients.h>
#include <pcl/features/integral_image_normal.h>
#include <pcl/features/normal_3d.h>
#include <pcl/io/pcd_io.h>
#include <pcl/surface/concave_hull.h>
#include <pcl_ros/transforms.h>
#include <pcl_ros/point_cloud.h>
#include <image_transport/image_transport.h>
#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/image_encodings.h>
#include <sensor_msgs/PointCloud2.h>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <iostream>
#include <cmath> 

using namespace sensor_msgs;
using namespace message_filters;
using namespace pcl;

ros::Publisher final_pub;
ros::Publisher left_pub;
ros::Publisher right_pub;

// PCL Visualizer to view the pointcloud
// pcl::visualization::PCLVisualizer viewer ("Door handle final");
  
// Initialise global Poincloud and Pointcloud pointer 8or else they dont work)
pcl::PointCloud<pcl::PointXYZ> output_cloud;
pcl::PointCloud<pcl::PointXYZ>::Ptr point_cloud_ptr(&output_cloud);

void callback(const sensor_msgs::PointCloud2ConstPtr& rgbd, const sensor_msgs::PointCloud2ConstPtr& pcl)
{
/*
  // Transform rgbd_pointcloud to XYZ format
  pcl::PCLPointCloud2* cloud_rgbd = new pcl::PCLPointCloud2; 
  pcl::PCLPointCloud2ConstPtr cloudPtr_rgbd(cloud_rgbd);
  pcl_conversions::toPCL(*rgbd, *cloud_rgbd);
  pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_rgbd_xyz(new pcl::PointCloud<pcl::PointXYZ>);
  pcl::fromPCLPointCloud2(*cloudPtr_rgbd, *cloud_rgbd_xyz);
  
  // Transform pcl_pointcloud to XYZ format
  pcl::PCLPointCloud2* cloud_pcl = new pcl::PCLPointCloud2; 
  pcl::PCLPointCloud2ConstPtr cloudPtr_pcl(cloud_pcl);
  pcl_conversions::toPCL(*pcl, *cloud_pcl);
  pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_pcl_xyz(new pcl::PointCloud<pcl::PointXYZ>);
  pcl::fromPCLPointCloud2(*cloudPtr_pcl, *cloud_pcl_xyz);
*/

  // Convert to XYZ type
  pcl::PointCloud<pcl::PointXYZ> cloud_rgbd;
  pcl::fromROSMsg(*rgbd, cloud_rgbd);
  pcl::PointCloud<pcl::PointXYZ> cloud_pcl;
  pcl::fromROSMsg(*pcl, cloud_pcl);

  // initialize output cloud and pointer

  
  

  output_cloud.clear();
  output_cloud.width = cloud_rgbd.size();
  output_cloud.height = 1;
  output_cloud.resize(cloud_rgbd.width);
  int counter = 0;
  int diff = 0.01;

  for (size_t i = 0; i < cloud_rgbd.size(); i++){
      bool in_cloud_pcl = false;
      for (size_t j = 0; j < cloud_pcl.size(); j++){
	  float x_diff = std::abs(cloud_rgbd.points[i].x - cloud_pcl.points[j].x);
	  float y_diff = std::abs(cloud_rgbd.points[i].y - cloud_pcl.points[j].y);
	  float z_diff = std::abs(cloud_rgbd.points[i].z - cloud_pcl.points[j].z);
          //std::cout<< " x_diff = " << x_diff << " y_diff = " << y_diff << " z_diff = " << z_diff << std::endl;

          if ((x_diff<0.04) && (y_diff<0.04) && (z_diff<0.01)){
              in_cloud_pcl = true;
              break;
          }
      }

      if (in_cloud_pcl){
          //std::cout<< " Saving " <<std::endl;
          output_cloud.points[counter].x = cloud_rgbd.points[i].x;
          output_cloud.points[counter].y = cloud_rgbd.points[i].y;
          output_cloud.points[counter].z = cloud_rgbd.points[i].z;
          counter++;
      }
  }
  output_cloud.width = counter;
  output_cloud.resize(counter);
  
  // Publish the data
  pcl_conversions::toPCL(ros::Time::now(),output_cloud.header.stamp);
  output_cloud.header.frame_id = "camera_rgb_optical_frame";
  final_pub.publish (output_cloud);
 
/*    
  // Visualize pointcloud
  viewer.addCoordinateSystem();
  viewer.addPointCloud (point_cloud_ptr, "scene_cloud");
  viewer.spinOnce();
  viewer.removePointCloud("scene_cloud");
*/

  // compute principal direction
  Eigen::Vector4f centroid;
  pcl::compute3DCentroid(*point_cloud_ptr, centroid);
  Eigen::Matrix3f covariance;
  computeCovarianceMatrixNormalized(*point_cloud_ptr, centroid, covariance);
  Eigen::SelfAdjointEigenSolver<Eigen::Matrix3f> eigen_solver(covariance, Eigen::ComputeEigenvectors);
  Eigen::Matrix3f eigDx = eigen_solver.eigenvectors();
  eigDx.col(2) = eigDx.col(0).cross(eigDx.col(1));

  // move the points to the that reference frame
  Eigen::Matrix4f p2w(Eigen::Matrix4f::Identity());
  p2w.block<3,3>(0,0) = eigDx.transpose();
  p2w.block<3,1>(0,3) = -1.f * (p2w.block<3,3>(0,0) * centroid.head<3>());
  pcl::PointCloud<PointXYZ> cPoints;
  pcl::transformPointCloud(*point_cloud_ptr, cPoints, p2w);

  PointXYZ min_pt, max_pt;
  pcl::getMinMax3D(cPoints, min_pt, max_pt);
  const Eigen::Vector3f mean_diag = 0.5f*(max_pt.getVector3fMap() + min_pt.getVector3fMap());

  // final transform
  const Eigen::Quaternionf qfinal(eigDx);
  const Eigen::Vector3f tfinal = eigDx*mean_diag + centroid.head<3>();


/*
  // draw the cloud and the box
  viewer.addCoordinateSystem();
  viewer.addPointCloud(point_cloud_ptr, "scene_cloud");
  viewer.addCube(tfinal, qfinal, max_pt.x - min_pt.x, max_pt.y - min_pt.y, max_pt.z - min_pt.z);
  viewer.spinOnce();
  viewer.removeAllShapes();
  viewer.removePointCloud("scene_cloud");
*/


  
  
  
  // If cloud is not empty, find points on the extremes of the handle
  if (point_cloud_ptr->width!=0)
  {
    float min_x;
    pcl::PointCloud<pcl::PointXYZ>::iterator min_it;
    float max_x;
    pcl::PointCloud<pcl::PointXYZ>::iterator max_it;
    pcl::PointXYZ LeftPoint;
    pcl::PointXYZ RightPoint;
    pcl::PointCloud<pcl::PointXYZ> left_point_pcl;
    pcl::PointCloud<pcl::PointXYZ> right_point_pcl;
 
    // Initialise min and max y values to the firtst point in the pointcloud
    pcl::PointCloud<pcl::PointXYZ>::iterator init = point_cloud_ptr->begin();
    max_x = init->x;
    min_x = init->x;
    RightPoint.x = init->x;
    RightPoint.y = init->y;
    RightPoint.z = init->z;
    LeftPoint.x = init->x;
    LeftPoint.y = init->y;
    LeftPoint.z = init->z;

    // Loop through the whole pointcloud to find max and min values of y
    for(pcl::PointCloud<pcl::PointXYZ>::iterator it = point_cloud_ptr->begin(); it
!= point_cloud_ptr->end(); it++)
    {
       if (it->x > max_x) 
       {
         max_x = it->x; 
         RightPoint.x = it->x;
         RightPoint.y = it->y;
         RightPoint.z = it->z;
       }
       if (it->x < min_x) 
       {
         min_x = it->x; 
         LeftPoint.x = it->x;
         LeftPoint.y = it->y;
         LeftPoint.z = it->z; 
       }
    }
        

    // Publish points
    left_point_pcl.points.push_back(LeftPoint);
    pcl_conversions::toPCL(ros::Time::now(),left_point_pcl.header.stamp);
    left_point_pcl.header.frame_id = "camera_rgb_optical_frame";
    left_pub.publish(left_point_pcl);
   
    
    right_point_pcl.points.push_back(RightPoint);
    pcl_conversions::toPCL(ros::Time::now(),right_point_pcl.header.stamp);
    right_point_pcl.header.frame_id = "camera_rgb_optical_frame";
    right_pub.publish(right_point_pcl);
    
  }


}

int main(int argc, char** argv)
{
  ros::init(argc, argv, "Final_handle");
  
  ros::NodeHandle nh;
  message_filters::Subscriber<sensor_msgs::PointCloud2> rgbd_sub(nh, "/door_detection/handle_rgbd", 1);
  message_filters::Subscriber<sensor_msgs::PointCloud2> pcl_sub(nh, "/door_detection/handle_pcl", 1);
  final_pub = nh.advertise<sensor_msgs::PointCloud2> ("/door_detection/handle_final", 1);
  left_pub = nh.advertise<sensor_msgs::PointCloud2> ("/door_detection/handle_final_left", 1);
  right_pub = nh.advertise<sensor_msgs::PointCloud2> ("/door_detection/handle_final_right", 1);

  typedef sync_policies::ApproximateTime<sensor_msgs::PointCloud2, sensor_msgs::PointCloud2> MySyncPolicy;
  // ApproximateTime takes a queue size as its constructor argument, hence MySyncPolicy(10)
  Synchronizer<MySyncPolicy> sync(MySyncPolicy(1), rgbd_sub, pcl_sub);
  sync.registerCallback(boost::bind(&callback, _1, _2));


  ros::spin ();
  return 0;

}
