#include "Camera.h"

#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace
{
	constexpr float PitchLimit = 1.5f;   // 약 86도. 정면 위/아래에서 시점이 뒤집히는 것을 막는다.

	bool IsKeyDown(int virtualKey)
	{
		return (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
	}
}

void Camera::SetPerspective(float fovYRadians, float aspect, float nearZ, float farZ)
{
	m_fovY = fovYRadians;
	m_aspect = aspect;
	m_nearZ = nearZ;
	m_farZ = farZ;
}

void Camera::LookAt(const XMFLOAT3& target)
{
	const XMVECTOR direction = XMVector3Normalize(
		XMVectorSubtract(XMLoadFloat3(&target), XMLoadFloat3(&m_position)));

	XMFLOAT3 forward = {};
	XMStoreFloat3(&forward, direction);

	m_pitch = std::asin(std::clamp(forward.y, -1.0f, 1.0f));
	m_yaw = std::atan2(forward.x, forward.z);
}

XMVECTOR Camera::GetForward() const
{
	const float cosPitch = std::cos(m_pitch);
	return XMVectorSet(cosPitch * std::sin(m_yaw), std::sin(m_pitch), cosPitch * std::cos(m_yaw), 0.0f);
}

void Camera::Update(float deltaTime)
{
	const XMVECTOR forward = GetForward();
	const XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	const XMVECTOR right = XMVector3Normalize(XMVector3Cross(worldUp, forward));

	XMVECTOR movement = XMVectorZero();
	if (IsKeyDown('W')) movement = XMVectorAdd(movement, forward);
	if (IsKeyDown('S')) movement = XMVectorSubtract(movement, forward);
	if (IsKeyDown('D')) movement = XMVectorAdd(movement, right);
	if (IsKeyDown('A')) movement = XMVectorSubtract(movement, right);
	if (IsKeyDown('E')) movement = XMVectorAdd(movement, worldUp);
	if (IsKeyDown('Q')) movement = XMVectorSubtract(movement, worldUp);

	if (XMVector3Equal(movement, XMVectorZero()))
	{
		return;
	}

	const float speed = m_moveSpeed * (IsKeyDown(VK_SHIFT) ? 4.0f : 1.0f) * deltaTime;
	const XMVECTOR delta = XMVectorScale(XMVector3Normalize(movement), speed);
	XMStoreFloat3(&m_position, XMVectorAdd(XMLoadFloat3(&m_position), delta));
}

void Camera::OnMouseMove(int x, int y)
{
	// 오른쪽 버튼을 누른 동안만 회전한다. 버튼 상태는 메시지 대신 직접 조회해
	// Window 에 핸들러를 더 늘리지 않았다.
	if (!IsKeyDown(VK_RBUTTON))
	{
		m_dragging = false;
		return;
	}

	if (!m_dragging)
	{
		m_dragging = true;
		m_lastMouseX = x;
		m_lastMouseY = y;
		return;
	}

	const float deltaX = static_cast<float>(x - m_lastMouseX);
	const float deltaY = static_cast<float>(y - m_lastMouseY);
	m_lastMouseX = x;
	m_lastMouseY = y;

	m_yaw += deltaX * m_lookSpeed;
	m_pitch = std::clamp(m_pitch - deltaY * m_lookSpeed, -PitchLimit, PitchLimit);
}

XMMATRIX Camera::GetView() const
{
	return XMMatrixLookToLH(
		XMLoadFloat3(&m_position),
		GetForward(),
		XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
}

XMMATRIX Camera::GetProjection() const
{
	return XMMatrixPerspectiveFovLH(m_fovY, m_aspect, m_nearZ, m_farZ);
}

XMMATRIX Camera::GetViewProjection() const
{
	return XMMatrixMultiply(GetView(), GetProjection());
}
