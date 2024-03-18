/*
 * Copyright (c) 2023 TU Wien, Institute of Visual Computing & Human-Centered Technology.
 */
#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include "VulkanLaunchpad.h"

 /*!
 * Struct that contains all a camera's required data
 */
struct VklCamera
{
	glm::mat4 mViewMatrix;
	glm::mat4 mProjMatrix;
	double mMouseX, mMouseY;
	float mPitch, mYaw;
	glm::vec3 mPosition;
	GLFWwindow* mWindow;
	GLFWkeyfun mPreviousKeyFun;
};

/*!
* Handle to address a camera
*/
using VklCameraHandle = VklCamera*;

/*!
* Camera constructor, which internally creates GLFW hooks for handling mouse input.
* @param	window					The glfw window handle.
* @param	projection_matrix		The projection matrix to be used for this camera
* @return	A handle that uniquely identifies a camera
*/
VklCameraHandle vklCreateCamera(GLFWwindow* window, glm::mat4 projection_matrix);

/*!
* Camera constructor which internally creates a default projection matrix.
* Also internally creates GLFW hooks for handling mouse input.
* @param	window		The glfw window handle.
* @return	A handle that uniquely identifies a camera
*/
VklCameraHandle vklCreateCamera(GLFWwindow* window);

/*!
* Destroys a given camera, also unhooking GLFW callbacks and restoring the previous callbacks.
* @param	handle		Handle that uniquely indentifies a camera
*/
void vklDestroyCamera(VklCameraHandle handle);

/*!
* @param	handle		Handle that uniquely indentifies a camera
* @return the current position of the camera
*/
glm::vec3 vklGetCameraPosition(VklCameraHandle handle);

/*!
* @param	handle		Handle that uniquely indentifies a camera
* @return the view matrix
*/
glm::mat4 vklGetCameraViewMatrix(VklCameraHandle handle);

/*!
* @param	handle		Handle that uniquely indentifies a camera
* @return the projection matrix
*/
glm::mat4 vklGetCameraProjectionMatrix(VklCameraHandle handle);

/*!
* @param	handle		Handle that uniquely indentifies a camera
* @return the view-projection matrix
*/
glm::mat4 vklGetCameraViewProjectionMatrix(VklCameraHandle handle);

/*!
* Updates the camera's position and view matrix according to the input
* @param	handle		Handle that uniquely indentifies a camera
* @param	x			current mouse x position
* @param	y			current mouse x position
* @param	keyW		is W pressed
* @param	keyA		is A pressed
* @param	keyS		is S pressed
* @param	keyD		is D pressed
* @param	keySpace	is space pressed
* @param	keyCtrl		is left ctrl pressed
* @param	keyShift	is shift pressed
* @param	dt			Delta time
*/
void vklUpdateCamera(VklCameraHandle handle, double x, double y, bool keyW, bool keyA, bool keyS, bool keyD, bool keySpace, bool keyCtrl, bool keyShift, double dt);

/*!
* Updates the camera's position and view matrix with the current mouse input.
* Ensure that glfwPollEvents has been invoked before, s.t. updated user input is available.
* @param	handle		Handle that uniquely indentifies a camera
* @param	dt			Delta time
*/
void vklUpdateCamera(VklCameraHandle handle, double dt);
