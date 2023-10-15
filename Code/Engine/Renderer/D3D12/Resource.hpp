#pragma once

struct ID3D12Resource2;
struct ID3D12GraphicsCommandList;
enum D3D12_RESOURCE_STATES : int;

class Resource {
	friend class Renderer;
	friend class Texture;
public:
	void TransitionTo(D3D12_RESOURCE_STATES newState, ID3D12GraphicsCommandList* commList);
private:
	Resource();
	~Resource();

	ID3D12Resource2* m_resource;
	int m_currentState = 0;
};