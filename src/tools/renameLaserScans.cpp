#include "ros/ros.h"
#include "ros/console.h"

#include "pointmatcher/PointMatcher.h"
#include "pointmatcher/Timer.h"
#include "pointmatcher_ros/point_cloud.h"
#include "pointmatcher_ros/transform.h"
#include "nabo/nabo.h"
#include "eigen_conversions/eigen_msg.h"
#include "pointmatcher_ros/get_params_from_server.h"

#include <tf/transform_broadcaster.h>

#include <fstream>
#include <vector>
#include <algorithm>
#include <stdio.h>
#include <iomanip>

#include <visualization_msgs/Marker.h>
#include <geometry_msgs/PoseStamped.h>
#include <nav_msgs/Path.h>

#include <numeric>

using namespace std;

class renameLaserScans
{

public:
    renameLaserScans(ros::NodeHandle &n);
    ~renameLaserScans();
    ros::NodeHandle& n;

    bool isChery;
    bool isKITTI;

    string indexFileName;
    string saveScanDir;
    string loadScanDir;

    void process(int cnt);

};

renameLaserScans::~renameLaserScans()
{}

renameLaserScans::renameLaserScans(ros::NodeHandle& n):
    n(n),
    indexFileName(getParam<string>("indexFileName", ".")),
    loadScanDir(getParam<string>("loadScanDir", ".")),
    saveScanDir(getParam<string>("saveScanDir", "."))
{


}

void renameLaserScans::process(int cnt)
{

}

int main(int argc, char **argv)
{

    ros::init(argc, argv, "renameLaserScans");
    ros::NodeHandle n;

    renameLaserScans renameLaserScans_(n);

    // ugly code

    return 0;
}