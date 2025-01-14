// Copyright 2012 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/D3DCommandListManager.h"
#include "VideoBackends/D3D12/PerfQuery.h"
#include "VideoCommon/RenderBase.h"

//D3D12TODO: Implement PerfQuery class.

namespace DX12
{

PerfQuery::PerfQuery()
{
	D3D12_QUERY_HEAP_DESC desc = { D3D12_QUERY_HEAP_TYPE_OCCLUSION, PERF_QUERY_BUFFER_SIZE, 0 };
	CheckHR(D3D::device->CreateQueryHeap(&desc, IID_PPV_ARGS(&m_query_heap)));

	CheckHR(D3D::device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(QUERY_READBACK_BUFFER_SIZE),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&m_query_readback_buffer)));

	m_tracking_fence = D3D::command_list_mgr->RegisterQueueFenceCallback(this, &PerfQuery::QueueFenceCallback);
	ResetQuery();
}

PerfQuery::~PerfQuery()
{
	D3D::command_list_mgr->RemoveQueueFenceCallback(this);

	SAFE_RELEASE(m_query_heap);
	SAFE_RELEASE(m_query_readback_buffer);
}

void PerfQuery::EnableQuery(PerfQueryGroup type)
{
	if (m_query_count > m_query_buffer.size() / 2)
		WeakFlush();

	// all queries already used?
	if (m_query_buffer.size() == m_query_count)
	{
		FlushOne();
		ERROR_LOG(VIDEO, "Flushed query buffer early!");
	}

	if (type == PQG_ZCOMP_ZCOMPLOC || type == PQG_ZCOMP)
	{
		size_t index = (m_query_read_pos + m_query_count) % m_query_buffer.size();
		auto& entry = m_query_buffer[index];

		D3D::current_command_list->BeginQuery(m_query_heap, D3D12_QUERY_TYPE_OCCLUSION, static_cast<UINT>(index));
		entry.query_type = type;
		entry.fence_value = -1;

		++m_query_count;
	}
}

void PerfQuery::DisableQuery(PerfQueryGroup type)
{
	if (type == PQG_ZCOMP_ZCOMPLOC || type == PQG_ZCOMP)
	{
		size_t index = (m_query_read_pos + m_query_count + m_query_buffer.size() - 1) % m_query_buffer.size();
		auto& entry = m_query_buffer[index];

		D3D::current_command_list->EndQuery(m_query_heap, D3D12_QUERY_TYPE_OCCLUSION, static_cast<UINT>(index));
		D3D::current_command_list->ResolveQueryData(m_query_heap, D3D12_QUERY_TYPE_OCCLUSION, static_cast<UINT>(index), 1, m_query_readback_buffer, index * sizeof(UINT64));
		entry.fence_value = m_next_fence_value;
	}
}

void PerfQuery::ResetQuery()
{
	m_query_count = 0;
	std::fill_n(m_results, ArraySize(m_results), 0);
}

u32 PerfQuery::GetQueryResult(PerfQueryType type)
{
	u32 result = 0;

	if (type == PQ_ZCOMP_INPUT_ZCOMPLOC || type == PQ_ZCOMP_OUTPUT_ZCOMPLOC)
		result = m_results[PQG_ZCOMP_ZCOMPLOC];
	else if (type == PQ_ZCOMP_INPUT || type == PQ_ZCOMP_OUTPUT)
		result = m_results[PQG_ZCOMP];
	else if (type == PQ_BLEND_INPUT)
		result = m_results[PQG_ZCOMP] + m_results[PQG_ZCOMP_ZCOMPLOC];
	else if (type == PQ_EFB_COPY_CLOCKS)
		result = m_results[PQG_EFB_COPY_CLOCKS];

	return result / 4;
}

void PerfQuery::FlushOne()
{
	size_t index = m_query_read_pos;
	ActiveQuery& entry = m_query_buffer[index];

	// Has the command list been executed yet?
	if (entry.fence_value == m_next_fence_value)
		D3D::command_list_mgr->ExecuteQueuedWork(false);

	// Block until the fence is reached
	D3D::command_list_mgr->WaitOnCPUForFence(m_tracking_fence, entry.fence_value);

	// Copy from readback buffer to local
	D3D12_RANGE range = { sizeof(UINT64) * index, sizeof(UINT64) * (index + 1) };
	void* readback_buffer_map;
	CheckHR(m_query_readback_buffer->Map(0, &range, &readback_buffer_map));

	UINT64 result;
	memcpy(&result, reinterpret_cast<u8*>(readback_buffer_map) + sizeof(UINT64) * index, sizeof(UINT64));

	D3D12_RANGE write_range = {};
	m_query_readback_buffer->Unmap(0, &write_range);

	// NOTE: Reported pixel metrics should be referenced to native resolution
	m_results[entry.query_type] += (u32)(result * EFB_WIDTH / g_renderer->GetTargetWidth() * EFB_HEIGHT / g_renderer->GetTargetHeight());

	m_query_read_pos = (m_query_read_pos + 1) % m_query_buffer.size();
	--m_query_count;
}

void PerfQuery::FlushResults()
{
	// TODO: Optimize this.
	while (!IsFlushed())
		FlushOne();
}

void PerfQuery::WeakFlush()
{
	UINT64 completed_fence = m_tracking_fence->GetCompletedValue();

	while (!IsFlushed())
	{
		ActiveQuery& entry = m_query_buffer[m_query_read_pos];
		if (entry.fence_value > completed_fence)
			break;

		FlushOne();
	}
}

bool PerfQuery::IsFlushed() const
{
	return m_query_count == 0;
}

void PerfQuery::QueueFenceCallback(void* owning_object, UINT64 fence_value)
{
	PerfQuery* owning_perf_query = static_cast<PerfQuery*>(owning_object);
	owning_perf_query->QueueFence(fence_value);
}

void PerfQuery::QueueFence(UINT64 fence_value)
{
	m_next_fence_value = fence_value + 1;
}

} // namespace
