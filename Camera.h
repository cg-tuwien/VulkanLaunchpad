/*
 * Copyright 2021 TU Wien, Institute of Visual Computing & Human-Centered Technology.
 * This file is part of the ECG Lab Framework and must not be redistributed.
 *
 * Original version created by Lukas Gersthofer and Bernhard Steiner.
 * Vulkan edition created by Johannes Unterguggenberger (junt@cg.tuwien.ac.at).
 */
#pragma once


#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

/*!
 * Arc ball camera, modified by mouse input
 */
class Camera
{	
public:
	/*!
	 * Camera constructor
	 * @param	projection_matrix		The projection matrix to be used for this camera
	 */
	Camera(glm::mat4 projection_matrix);
	
	virtual ~Camera();

	/*!
	 * @return the current position of the camera
	 */
	glm::vec3 getPosition() const;

	/*!
	 * @return the view-projection matrix
	 */
	glm::mat4 getViewProjectionMatrix() const;

		/*!
	 * Updates the camera's position and view matrix according to the input
	 * @param x			current mouse x position
	 * @param y			current mouse x position
	 * @param zoom		zoom multiplier
	 * @param dragging	is the camera dragging
	 * @param strafing	is the camera strafing
	 */
	void update(double x, double y, float zoom, bool dragging, bool strafing);

protected:
	glm::mat4 _viewMatrix;
	glm::mat4 _projMatrix;
	double _mouseX, _mouseY;
	float ooo, ggg;
	glm::vec3 _position;
	glm::vec3 _strafe;
	glm::vec3 ttt;
	glm::vec3 tt;
};