/*
 * PnPProblem.cpp
 *
 *  Created on: Mar 28, 2014
 *      Author: Edgar Riba
 */

#include <iostream>
#include <sstream>
#include <string>
#include <opencv2/calib3d/calib3d.hpp>

#include "PnPProblem.h"
#include "Mesh.h"
//#include "mat.h"
#include <vector>
#include<fstream>
#include <chrono>
#include <thread>

#include <opencv2/calib3d/calib3d.hpp>

using namespace std;

const string filepath_r = "OPnP_R.txt";
const string filepath_t = "OPnP_t.txt";
const string points_path = "3dpoints.txt";
const string pixels_path = "2dpoints.txt";

/* Functions for Möller-Trumbore intersection algorithm */
static cv::Point3f CROSS(cv::Point3f v1, cv::Point3f v2)
{
    cv::Point3f tmp_p;
    tmp_p.x =  v1.y*v2.z - v1.z*v2.y;
    tmp_p.y =  v1.z*v2.x - v1.x*v2.z;
    tmp_p.z =  v1.x*v2.y - v1.y*v2.x;
    return tmp_p;
}

static double DOT(cv::Point3f v1, cv::Point3f v2)
{
    return v1.x*v2.x + v1.y*v2.y + v1.z*v2.z;
}

static cv::Point3f SUB(cv::Point3f v1, cv::Point3f v2)
{
    cv::Point3f tmp_p;
    tmp_p.x =  v1.x - v2.x;
    tmp_p.y =  v1.y - v2.y;
    tmp_p.z =  v1.z - v2.z;
    return tmp_p;
}

/* End functions for Möller-Trumbore intersection algorithm */

// Function to get the nearest 3D point to the Ray origin
static cv::Point3f get_nearest_3D_point(vector<cv::Point3f> &points_list, cv::Point3f origin)
{
    cv::Point3f p1 = points_list[0];
    cv::Point3f p2 = points_list[1];

    double d1 = sqrt( pow(p1.x-origin.x, 2) + pow(p1.y-origin.y, 2) + pow(p1.z-origin.z, 2) );
    double d2 = sqrt( pow(p2.x-origin.x, 2) + pow(p2.y-origin.y, 2) + pow(p2.z-origin.z, 2) );

    if(d1 < d2)
    {
        return p1;
    }
    else
    {
        return p2;
    }
}


cv::Mat ReadMatFromTxt(const string filename, int rows, int cols)
{
    double m;
    char a;
    cv::Mat out = cv::Mat::zeros(rows, cols, CV_64FC1);//Matrix to store values

    ifstream fileStream(filename);
    int cnt = 0;//index starts from 0
    while (fileStream >> m)
    {
        int temprow = cnt / cols;
        int tempcol = cnt % cols;
        out.at<double>(temprow, tempcol) = m;
        cnt++;
        fileStream >> a;
    }
    return out;
}


void writeMatToFile(cv::Mat& m, const string filename)
{
    ofstream fout(filename);

    if (!fout)
    {
        cout << "File Not Opened" << endl;  return;
    }

    for (int i = 0; i < m.rows; i++)
    {
        for (int j = 0; j < m.cols; j++)
        {
            fout << m.at<double>(i, j);
            if(j < m.cols - 1)
                fout << " ";
        }
        fout << endl;
    }

    fout.close();
}

// Custom constructor given the intrinsic camera parameters

PnPProblem::PnPProblem(const double params[])
{
    A_matrix_ = cv::Mat::zeros(3, 3, CV_64FC1);   // intrinsic camera parameters
    A_matrix_.at<double>(0, 0) = params[0];       //      [ fx   0  cx ]
    A_matrix_.at<double>(1, 1) = params[1];       //      [  0  fy  cy ]
    A_matrix_.at<double>(0, 2) = params[2];       //      [  0   0   1 ]
    A_matrix_.at<double>(1, 2) = params[3];
    A_matrix_.at<double>(2, 2) = 1;
    R_matrix_ = cv::Mat::zeros(3, 3, CV_64FC1);   // rotation matrix
    t_matrix_ = cv::Mat::zeros(3, 1, CV_64FC1);   // translation matrix
    P_matrix_ = cv::Mat::zeros(3, 4, CV_64FC1);   // rotation-translation matrix

}

PnPProblem::~PnPProblem()
{
    // TODO Auto-generated destructor stub
}

void PnPProblem::set_P_matrix( const cv::Mat &R_matrix, const cv::Mat &t_matrix)
{
    // Rotation-Translation Matrix Definition
    P_matrix_.at<double>(0,0) = R_matrix.at<double>(0,0);
    P_matrix_.at<double>(0,1) = -R_matrix.at<double>(0,1);
    P_matrix_.at<double>(0,2) = R_matrix.at<double>(0,2);
    P_matrix_.at<double>(1,0) = R_matrix.at<double>(1,0);
    P_matrix_.at<double>(1,1) = R_matrix.at<double>(1,1);
    P_matrix_.at<double>(1,2) = R_matrix.at<double>(1,2);
    P_matrix_.at<double>(2,0) = R_matrix.at<double>(2,0);
    P_matrix_.at<double>(2,1) = R_matrix.at<double>(2,1);
    P_matrix_.at<double>(2,2) = R_matrix.at<double>(2,2);
    P_matrix_.at<double>(0,3) = t_matrix.at<double>(0);
    P_matrix_.at<double>(1,3) = t_matrix.at<double>(1);
    P_matrix_.at<double>(2,3) = t_matrix.at<double>(2);
}

// Estimate the pose given a list of 2D/3D correspondences and the method to use
bool PnPProblem::estimatePose(const std::vector<cv::Point3f> &list_points3d,
	const std::vector<cv::Point2f> &list_points2d,
	int flags)
{
	cv::Mat distCoeffs = cv::Mat::zeros(4, 1, CV_64FC1);
	cv::Mat rvec = cv::Mat::zeros(3, 1, CV_64FC1);
	cv::Mat tvec = cv::Mat::zeros(3, 1, CV_64FC1);

	bool useExtrinsicGuess = false;

	// Pose estimation
	bool correspondence = cv::solvePnP(list_points3d, list_points2d, A_matrix_, distCoeffs, rvec, tvec,
		useExtrinsicGuess, flags);

	// Transforms Rotation Vector to Matrix
	Rodrigues(rvec, R_matrix_);
	t_matrix_ = tvec;

	// Set projection matrix
	this->set_P_matrix(R_matrix_, t_matrix_);

	return correspondence;
}

// Estimate the pose given a list of 2D/3D correspondences with RANSAC and the method to use

void PnPProblem::estimatePoseRANSAC( const vector<cv::Point3f> &list_points3d, // list with model 3D coordinates
                                     const vector<cv::Point2f> &list_points2d,     // list with scene 2D coordinates
                                     int flags, cv::Mat &inliers, int iterationsCount,  // PnP method; inliers container
                                     float reprojectionError, double confidence )    // Ransac parameters
{
    cv::Mat distCoeffs = cv::Mat::zeros(4, 1, CV_64FC1);  // vector of distortion coefficients
    cv::Mat rvec = cv::Mat::zeros(3, 3, CV_64FC1);          // output rotation vector
    cv::Mat tvec = cv::Mat::zeros(3, 1, CV_64FC1);    // output translation vector

    bool useExtrinsicGuess = false;   // if true the function uses the provided rvec and tvec values as
    // initial approximations of the rotation and translation vectors

	cv::solvePnPRansac( list_points3d, list_points2d, A_matrix_, distCoeffs, rvec, tvec,
                        useExtrinsicGuess, iterationsCount, reprojectionError, confidence,
                        inliers, flags );

	///
            int sizePoints = inliers.rows;
            cout << list_points3d.size() << " " << sizePoints << endl;
            cv::Mat matPoints = cv::Mat::zeros(sizePoints, 3, CV_64FC1);
            cv::Mat matPixels = cv::Mat::zeros(sizePoints, 2, CV_64FC1);

            for (int i = 0; i < sizePoints; i++) {
                matPoints.at<double>(i, 0) = list_points3d[inliers.at<int>(i)].x;
                matPoints.at<double>(i, 1) = list_points3d[inliers.at<int>(i)].y;
                matPoints.at<double>(i, 2) = list_points3d[inliers.at<int>(i)].z;
                matPixels.at<double>(i, 0) = list_points2d[inliers.at<int>(i)].x;
                matPixels.at<double>(i, 1) = list_points2d[inliers.at<int>(i)].y;
            }

            writeMatToFile(matPoints, points_path);
            writeMatToFile(matPixels, pixels_path);

            
            ofstream fout;

            std::cout << "***********************************" << std::endl;
            system("python3 run.py > /dev/null");
            //Got R & t 
            
            rvec = ReadMatFromTxt(filepath_r, 3, 3);
            tvec = ReadMatFromTxt(filepath_t, 3, 1);

			R_matrix_ = rvec.t(); 				// converts Rotation Vector to Matrix
		    t_matrix_ = tvec;           // set translation matrix

		    cout << R_matrix_ << endl;
		    cout << t_matrix_ << endl;

    this->set_P_matrix(R_matrix_, t_matrix_); // set rotation-translation matrix

}

// Given the mesh, backproject the 3D points to 2D to verify the pose estimation
vector<cv::Point2f> PnPProblem::verify_points(Mesh *mesh)
{
    vector<cv::Point2f> verified_points_2d;
    for( int i = 0; i < mesh->getNumVertices(); i++)
    {
        cv::Point3f point3d = mesh->getVertex(i);
        cv::Point2f point2d = this->backproject3DPoint(point3d);
        verified_points_2d.push_back(point2d);
    }

    return verified_points_2d;
}

// Backproject a 3D point to 2D using the estimated pose parameters
cv::Point2f PnPProblem::backproject3DPoint(const cv::Point3f &point3d)
{
    // 3D point vector [x y z 1]'
    cv::Mat point3d_vec = cv::Mat(4, 1, CV_64FC1);
    point3d_vec.at<double>(0) = point3d.x;
    point3d_vec.at<double>(1) = point3d.y;
    point3d_vec.at<double>(2) = point3d.z;
    point3d_vec.at<double>(3) = 1;

    // 2D point vector [u v 1]'
    cv::Mat point2d_vec = cv::Mat(4, 1, CV_64FC1);
    point2d_vec = A_matrix_ * P_matrix_ * point3d_vec;

    // Normalization of [u v]'
    cv::Point2f point2d;
    point2d.x = (float)(point2d_vec.at<double>(0) / point2d_vec.at<double>(2));
    point2d.y = (float)(point2d_vec.at<double>(1) / point2d_vec.at<double>(2));

    return point2d;
}

// Back project a 2D point to 3D and returns if it's on the object surface
bool PnPProblem::backproject2DPoint(const Mesh *mesh, const cv::Point2f &point2d, cv::Point3f &point3d)
{
    // Triangles list of the object mesh
    vector<vector<int> > triangles_list = mesh->getTrianglesList();

    double lambda = 8;
    double u = point2d.x;
    double v = point2d.y;

    // Point in vector form
    cv::Mat point2d_vec = cv::Mat::ones(3, 1, CV_64F); // 3x1
    point2d_vec.at<double>(0) = u * lambda;
    point2d_vec.at<double>(1) = v * lambda;
    point2d_vec.at<double>(2) = lambda;

    // Point in camera coordinates
    cv::Mat X_c = A_matrix_.inv() * point2d_vec ; // 3x1

    // Point in world coordinates
    cv::Mat X_w = R_matrix_.inv() * ( X_c - t_matrix_ ); // 3x1

    // Center of projection
    cv::Mat C_op = cv::Mat(R_matrix_.inv()).mul(-1) * t_matrix_; // 3x1

    // Ray direction vector
    cv::Mat ray = X_w - C_op; // 3x1
    ray = ray / cv::norm(ray); // 3x1

    // Set up Ray
    Ray R((cv::Point3f)C_op, (cv::Point3f)ray);

    // A vector to store the intersections found
    vector<cv::Point3f> intersections_list;

    // Loop for all the triangles and check the intersection
    for (unsigned int i = 0; i < triangles_list.size(); i++)
    {
        cv::Point3f V0 = mesh->getVertex(triangles_list[i][0]);
        cv::Point3f V1 = mesh->getVertex(triangles_list[i][1]);
        cv::Point3f V2 = mesh->getVertex(triangles_list[i][2]);

        Triangle T(V0, V1, V2);

        double out;
        if(this->intersect_MollerTrumbore(R, T, &out))
        {
            cv::Point3f tmp_pt = R.getP0() + out*R.getP1(); // P = O + t*D
            intersections_list.push_back(tmp_pt);
        }
    }

    // If there are intersection, find the nearest one
    if (!intersections_list.empty())
    {
        point3d = get_nearest_3D_point(intersections_list, R.getP0());
        return true;
    }
    else
    {
        return false;
    }
}

// Möller-Trumbore intersection algorithm
bool PnPProblem::intersect_MollerTrumbore(Ray &Ray, Triangle &Triangle, double *out)
{
    const double EPSILON = 0.000001;

    cv::Point3f e1, e2;
    cv::Point3f P, Q, T;
    double det, inv_det, u, v;
    double t;

    cv::Point3f V1 = Triangle.getV0();  // Triangle vertices
    cv::Point3f V2 = Triangle.getV1();
    cv::Point3f V3 = Triangle.getV2();

    cv::Point3f O = Ray.getP0(); // Ray origin
    cv::Point3f D = Ray.getP1(); // Ray direction

    //Find vectors for two edges sharing V1
    e1 = SUB(V2, V1);
    e2 = SUB(V3, V1);

    // Begin calculation determinant - also used to calculate U parameter
    P = CROSS(D, e2);

    // If determinant is near zero, ray lie in plane of triangle
    det = DOT(e1, P);

    //NOT CULLING
    if(det > -EPSILON && det < EPSILON) return false;
    inv_det = 1.f / det;

    //calculate distance from V1 to ray origin
    T = SUB(O, V1);

    //Calculate u parameter and test bound
    u = DOT(T, P) * inv_det;

    //The intersection lies outside of the triangle
    if(u < 0.f || u > 1.f) return false;

    //Prepare to test v parameter
    Q = CROSS(T, e1);

    //Calculate V parameter and test bound
    v = DOT(D, Q) * inv_det;

    //The intersection lies outside of the triangle
    if(v < 0.f || u + v  > 1.f) return false;

    t = DOT(e2, Q) * inv_det;

    if(t > EPSILON) { //ray intersection
        *out = t;
        return true;
    }

    // No hit, no win
    return false;
}