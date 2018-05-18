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

#include <visualization_msgs/Marker.h>
#include <geometry_msgs/PoseStamped.h>
#include <nav_msgs/Path.h>

#include <numeric>

using namespace std;
using namespace PointMatcherSupport;

class labelLaser
{
    typedef PointMatcher<float> PM;
    typedef PM::DataPoints DP;
    typedef PM::Matches Matches;

    typedef typename Nabo::NearestNeighbourSearch<float> NNS;
    typedef typename NNS::SearchType NNSearchType;

public:
    labelLaser(ros::NodeHandle &n);
    ~labelLaser();
    ros::NodeHandle& n;

    string loadMapName;
    string icpFileName;
    string velodyneDirName;
    int totalICPnum;
    string saveTxtDirName;
    double horizontalResRad;

    DP mapCloud;
    DP velodyneCloud;

    int rowLineSession;
    shared_ptr<NNS> featureNNS;
    vector<vector<double>> initPoses;
    unique_ptr<PM::Transformation> transformation;
    PM::TransformationParameters Trobot;
    double rangeFilter;


    void wait();
    void process(int index);
    DP readBin(string fileName);
    double getRangeOfPoint(double x, double y, double z);

};

labelLaser::~labelLaser()
{}

labelLaser::labelLaser(ros::NodeHandle& n):
    n(n),
    loadMapName(getParam<string>("loadMapName", ".")),
    icpFileName(getParam<string>("icpFileName", ".")),
    velodyneDirName(getParam<string>("velodyneDirName", ".")),
    totalICPnum(getParam<int>("totalICPnum", 0)),
    saveTxtDirName(getParam<string>("saveTxtDirName", ".")),
    horizontalResRad(getParam<double>("horizontalResRad", 0)),
    transformation(PM::get().REG(Transformation).create("RigidTransformation"))
{

    // load
    mapCloud = DP::load(loadMapName);
    rowLineSession = mapCloud.getDescriptorStartingRow("session");

    featureNNS.reset(NNS::create(mapCloud.features, mapCloud.features.rows() - 1, NNS::KDTREE_LINEAR_HEAP, NNS::TOUCH_STATISTICS));

    // read initial transformation
    int x, y;
    double temp;
    vector<double> test;
    ifstream in(icpFileName);
    if (!in) {
        cout << "Cannot open file.\n";
    }
    for (y = 0; y < totalICPnum; y++) {
        test.clear();
    for (x = 0; x < 16; x++) {
      in >> temp;
      test.push_back(temp);
    }
      initPoses.push_back(test);
    }
    in.close();

    // process
    int index = 0;
    for(; index < this->totalICPnum; index++)
    {
        this->process(index);
        cout<<"-------------------------------------------------------"<<endl;
    }

    this->wait();
}

void labelLaser::wait()
{
    ros::Rate r(1);

    while(ros::ok())
    {
    }
}

void labelLaser::process(int index)
{
    // loading velodyne
    stringstream ss;
    ss<<setw(10)<<setfill('0')<<index;
    string str;
    ss>>str;
    string veloName = velodyneDirName + str + ".bin";
    cout<<veloName<<endl;

    velodyneCloud = readBin(veloName);

    Trobot = PM::TransformationParameters::Identity(4, 4);
    Trobot(0,0)=initPoses[index][0];Trobot(0,1)=initPoses[index][1];Trobot(0,2)=initPoses[index][2];Trobot(0,3)=initPoses[index][3];
    Trobot(1,0)=initPoses[index][4];Trobot(1,1)=initPoses[index][5];Trobot(1,2)=initPoses[index][6];Trobot(1,3)=initPoses[index][7];
    Trobot(2,0)=initPoses[index][8];Trobot(2,1)=initPoses[index][9];Trobot(2,2)=initPoses[index][10];Trobot(2,3)=initPoses[index][11];
    Trobot(3,0)=initPoses[index][12];Trobot(3,1)=initPoses[index][13];Trobot(3,2)=initPoses[index][14];Trobot(3,3)=initPoses[index][15];

    transformation->correctParameters(Trobot);

    if(transformation->checkParameters(Trobot))
    {
        DP velodyneCloud_ = transformation->compute(velodyneCloud, Trobot);

        PM::Matches matches_overlap(
            Matches::Dists(1, velodyneCloud_.features.cols()),
            Matches::Ids(1, velodyneCloud_.features.cols())
        );

        featureNNS->knn(velodyneCloud_.features, matches_overlap.ids, matches_overlap.dists, 1, 0);

        // save the results
        string saveTxtName = saveTxtDirName + str + ".txt";
        ofstream saveTxtStream(saveTxtName);

        for(int m=0; m<velodyneCloud_.features.cols(); m++)
        {
            // define the rangeFilter
            rangeFilter = this->getRangeOfPoint(velodyneCloud.features(0,m), velodyneCloud.features(1,m), velodyneCloud.features(2,m))
                          * this->horizontalResRad;

            if(matches_overlap.dists(0, m) > rangeFilter)
            {
                saveTxtStream<<0<<endl;
                saveTxtStream.flush();
            }
            else
            {
                saveTxtStream<<mapCloud.descriptors(rowLineSession, matches_overlap.ids(0, m))<<endl;
                saveTxtStream.flush();
            }


        }

        saveTxtStream.close();
    }


}

double labelLaser::getRangeOfPoint(double x, double y, double z)
{
    return sqrt(pow(x,2) + pow(y,2) + pow(z,2));
}

labelLaser::DP labelLaser::readBin(string filename)
{
    DP data;

    fstream input(filename.c_str(), ios::in | ios::binary);
    if(!input.good()){
        cerr << "Could not read file: " << filename << endl;
        exit(EXIT_FAILURE);
    }
    input.seekg(0, ios::beg);

    data.addFeature("x",PM::Matrix::Constant(1,300000,0));
    data.addFeature("y",PM::Matrix::Constant(1,300000,0));
    data.addFeature("z",PM::Matrix::Constant(1,300000,0));
    data.addFeature("pad",PM::Matrix::Constant(1,300000,1));
    data.addDescriptor("intensity",PM::Matrix::Constant(1,300000,0));
    data.addDescriptor("ring",PM::Matrix::Constant(1,300000,0));

    int i;

    for (i=0; input.good() && !input.eof(); i++) {
        float a,b;
        input.read((char *) &a, 4*sizeof(unsigned char));
        data.features(0,i) = a;
        input.read((char *) &a, 4*sizeof(unsigned char));
        data.features(1,i) = a;
        input.read((char *) &a, 4*sizeof(unsigned char));
        data.features(2,i) = a;
        input.read((char *) &b, 4*sizeof(unsigned char));
        input.read((char *) &a, 4*sizeof(unsigned char));
        data.descriptors(0,i) = a;
        input.read((char *) &a, 2*sizeof(unsigned char));
        data.descriptors(1,i) = a;

        input.read((char *) &b, 10*sizeof(unsigned char));

    }
    input.close();
    data.conservativeResize(i);

    return data;
}

int main(int argc, char **argv)
{

    ros::init(argc, argv, "labelLaser");
    ros::NodeHandle n;

    labelLaser labellaser(n);

    // ugly code

    return 0;
}