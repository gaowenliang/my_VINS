//
//  Camera.cpp
//  MyTriangulation
//
//  Created by Yang Shuo on 18/3/15.
//  Copyright (c) 2015 Yang Shuo. All rights reserved.
//

#include <math.h>
#include <iostream>
#include "Camera.h"
#include "math_tool.h"
using namespace std;
using namespace Eigen;
DistortCamera::DistortCamera()
{
    K = Matrix3f::Identity(3,3);
    optical = Vector2f::Zero(2,1);
    focusMtx = Matrix2f::Zero(2,2);
}

void DistortCamera::setImageSize(float _height, float _width)
{
    width = _width;
    nCols = _width;
    
    height = _height;
    nRows = _height;
}
void DistortCamera::setIntrinsicMtx(float _fx, float _fy, float _ox, float _oy)
{
    fx = _fx;
    fy = _fy;
    ox = _ox;
    oy = _oy;
    K(0,0) = fx;
    K(0,2) = ox;
    K(1,1) = fy;
    K(1,2) = oy;
    
    optical(0) = ox;
    optical(1) = oy;
    
    focusMtx(0,0) = fx;
    focusMtx(1,0) = fy;
}
void DistortCamera::setDistortionParam(float _k1, float _k2, float _p1, float _p2, float _k3)
{
    k1 = _k1;
    k2 = _k2;
    k3 = _k3;
    p1 = _p1;
    p2 = _p2;
}

Vector2f DistortCamera::h(Vector3f ptr)
{
    Vector2f z, dt, uv;
    float u, v, r, dr;
    u = ptr(0)/ptr(2);
    v = ptr(1)/ptr(2);
    
    r = u*u + v*v;
    dr = 1 + k1*r*r + k2*powf(r,4) + k3* powf(r, 6);
    
    dt(0) = 2*u*v*p1+(r+2*u*u)*p2;
    dt(1) = 2*u*v*p2+(r+2*v*v)*p1;
    
    uv(0) = u;
    uv(1) = v;
    z = optical + focusMtx*(dr * uv + dt);
    
//    cout << "dr is" << dr << endl;
//    cout << "dt is" << dt << endl;
//    cout << p1 << endl;
//    cout << p2 << endl;
//    cout << u << endl;
//    cout << v << endl;
    return z;
}


MatrixXf DistortCamera::Jh(Vector3f ptr)
{
    Vector2f dt;
    Matrix2f focus;
    MatrixXf J(2,3);
    MatrixXf Jdistort(2,3);
    float x, y, z, u, v, r, dr, x2, y2, z2, z3;
    float ddrdx, ddrdy, ddrdz;
    Vector2f ddtdx, ddtdy, ddtdz;
    
    focus(0,0) = fx;
    focus(1,1) = fy;
    
    x = ptr(0);
    y = ptr(1);
    z = ptr(2);
    x2 = x*x;
    y2 = y*y;
    z2 = z*z;
    z3 = z*z*z;
    
    u = x/z;
    v = y/z;
    
    r = u*u + v*v;
    
    dr = 1 + k1*r*r + k2*powf(r,4) + k3* powf(r, 6);
    
    dt(0) = 2*u*v*p1+(r+2*u*u)*p2;
    dt(1) = 2*u*v*p2+(r+2*v*v)*p1;
    
    ddrdx = (4*k1*x*(x2/z2 + y2/z2))/z2 + (8*k2*x*powf(x2/z2 + y2/z2,3))/z2 + (12*k3*x*powf(x2/z2 + y2/z2,5))/z2;
    ddrdy = (4*k1*y*(x2/z2 + y2/z2))/z2 + (8*k2*y*powf(x2/z2 + y2/z2,3))/z2 + (12*k3*y*powf(x2/z2 + y2/z2,5))/z2;
    ddrdz = - 4*k2*powf(x2/z2 + y2/z2,3) *
              ((2*x2)/z3 + (2*y2)/z3) - 6*k3*powf(x2/z2 + y2/z2,5)*((2*x2)/z3 + (2*y2)/z3) - 2*k1*(x2/z2 + y2/z2)*((2*x2)/z3 + (2*y2)/z3);
    
    ddtdx(0) = (6*p2*x)/z2 + (2*p1*y)/z2;
    ddtdx(1) = (2*p1*x)/z2 + (2*p2*y)/z2;
    ddtdy(0) = (2*p1*x)/z2 + (2*p2*y)/z2;
    ddtdy(1) = (2*p2*x)/z2 + (6*p1*y)/z2;
    ddtdz(0) = - p2*((6*x2)/z3 + (2*y2)/z3) - (4*p1*x*y)/z3;
    ddtdz(1) = - p1*((2*x2)/z3 + (6*y2)/z3) - (4*p2*x*y)/z3;
    
    
    Jdistort = MatrixXf::Zero(2, 3);
    
    Jdistort(0,0) =    dr/z + ddrdx*u + ddtdx(0);
    Jdistort(0,1) =           ddrdy*u + ddtdy(0);
    Jdistort(0,2) = -dr/z*u + ddrdz*u + ddtdz(0);
    Jdistort(1,0) =           ddrdx*v + ddtdx(1);
    Jdistort(1,1) =    dr/z + ddrdy*v + ddtdy(1);
    Jdistort(1,2) = -dr/z*v + ddrdz*v + ddtdz(1);
    
    J = focus * Jdistort;
    
    return J;
}

// measure is 2f
Vector3f DistortCamera::triangulate(MatrixXf measure, MatrixXf pose)
{
    Vector3f return_pose = Vector3f(0.0f, 0.0f, 0.0f);
    int num_item = (int)pose.cols();
    
    MatrixXf q_list = MatrixXf::Zero(4, num_item);
    MatrixXf t_list = MatrixXf::Zero(3, num_item);
    
    Matrix3f R_wc0 = quaternion_to_R(pose.block<4,1>(0,0));
    Vector3f t_wc0 = pose.block<3,1>(4,0);
    
    Matrix3f R_wci = Matrix3f::Identity(3, 3);
    Vector3f t_wci = Vector3f::Zero(3, 1);
    Matrix3f R_c0ci, R_cic0;
    Vector3f t_c0ci, t_cic0;
    
    q_list.col(0) = Vector4f(1,0,0,0);
    t_list.col(0) = t_wci;
    
    // TODO: rewrite this piece use quaternion
    for (int i=1; i<num_item; i++)
    {
        R_wci = quaternion_to_R(pose.block<4,1>(0,i));
        t_wci = pose.block<3,1>(4,i);
        
        R_c0ci = R_wc0.transpose()*R_wci;
        t_c0ci = R_wc0.transpose()*(t_wci - t_wc0);
        R_cic0 = R_c0ci.transpose();
        t_cic0 = - R_c0ci.transpose()*t_c0ci;
        
        q_list.col(i) = R_to_quaternion(R_cic0);
        t_list.col(i) = t_cic0;
    }
    
//    cout << "q_list is" << endl << q_list << endl;
//    cout << "t_list is" << endl << t_list << endl;
    
    // init estimation
    Vector3f theta = Vector3f(0.1f, 0.1f, 0.1f);
    Vector3f g_ptr = Vector3f(0.0f, 0.0f, 0.0f);
    VectorXf f = VectorXf::Zero(num_item*2);
    MatrixXf J = MatrixXf::Zero(num_item*2,3);
    Matrix3f Jg = Matrix3f::Zero(3,3);
    MatrixXf Ji;
    MatrixXf A;
    MatrixXf b;
    for (int itr = 0; itr < 100; itr++)
    {
        Vector3f tmp_theta = Vector3f(theta(0), theta(1), 1);
        for (int i = 0; i < num_item; i++)
        {
            R_cic0 = quaternion_to_R(q_list.col(i));
            g_ptr = R_cic0*tmp_theta + theta(2)*t_list.col(i);
            
            f.segment(i*2, 2) = measure.col(i) - h(g_ptr);
            
            Jg.col(0) = R_cic0.col(0);
            Jg.col(1) = R_cic0.col(1);
            Jg.col(2) = t_list.col(i);
            
            Ji = -Jh(g_ptr) * Jg;
            J.block<2,3>(i*2,0) = Ji;
        }
        
        if (f.norm() < 5.0f)
        {
            break;
        }
        A = J.transpose()*J;
        b = J.transpose()*f;
        
        
//        cout << "f is" << endl << f << endl;
//        cout << "J is" << endl << J << endl;
//        cout << "solve is" << endl << A.ldlt().solve(b) << endl;
        
        //theta = theta - A.ldlt().solve(b);
        theta = theta - A.colPivHouseholderQr().solve(b);
        
    }
    
    return_pose(0) = theta(0)/theta(2);
    return_pose(1) = theta(1)/theta(2);
    return_pose(2) = 1.0f/theta(2);
    
    return_pose = R_wc0*return_pose+t_wc0;
    
    return return_pose;
    
}



