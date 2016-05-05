/*
 * Copyright 2015-2016 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SPIRV_CROSS_INTERNAL_INTERFACE_HPP
#define SPIRV_CROSS_INTERNAL_INTERFACE_HPP

// This file must only be included by the shader generated by spirv-cross!

#ifndef GLM_SWIZZLE
#define GLM_SWIZZLE
#endif

#ifndef GLM_FORCE_RADIANS
#define GLM_FORCE_RADIANS
#endif

#include <glm/glm.hpp>

#include <assert.h>
#include <stdint.h>
#include "external_interface.h"
#include "barrier.hpp"
#include "thread_group.hpp"
#include "sampler.hpp"
#include "image.hpp"

namespace internal
{
// Adaptor helpers to adapt GLSL access chain syntax to C++.
// Don't bother with arrays of arrays on uniforms ...
// Would likely need horribly complex variadic template munging.

template <typename T>
struct Interface
{
	enum
	{
		ArraySize = 1,
		Size = sizeof(T)
	};

	Interface()
	    : ptr(0)
	{
	}
	T &get()
	{
		assert(ptr);
		return *ptr;
	}

	T *ptr;
};

// For array types, return a pointer instead.
template <typename T, unsigned U>
struct Interface<T[U]>
{
	enum
	{
		ArraySize = U,
		Size = U * sizeof(T)
	};

	Interface()
	    : ptr(0)
	{
	}
	T *get()
	{
		assert(ptr);
		return ptr;
	}

	T *ptr;
};

// For case when array size is 1, avoid double dereference.
template <typename T>
struct PointerInterface
{
	enum
	{
		ArraySize = 1,
		Size = sizeof(T *)
	};
	enum
	{
		PreDereference = true
	};

	PointerInterface()
	    : ptr(0)
	{
	}

	T &get()
	{
		assert(ptr);
		return *ptr;
	}

	T *ptr;
};

// Automatically converts a pointer down to reference to match GLSL syntax.
template <typename T>
struct DereferenceAdaptor
{
	DereferenceAdaptor(T **ptr)
	    : ptr(ptr)
	{
	}
	T &operator[](unsigned index) const
	{
		return *(ptr[index]);
	}
	T **ptr;
};

// We can't have a linear array of T* since T* can be an abstract type in case of samplers.
// We also need a list of pointers since we can have run-time length SSBOs.
template <typename T, unsigned U>
struct PointerInterface<T[U]>
{
	enum
	{
		ArraySize = U,
		Size = sizeof(T *) * U
	};
	enum
	{
		PreDereference = false
	};
	PointerInterface()
	    : ptr(0)
	{
	}

	DereferenceAdaptor<T> get()
	{
		assert(ptr);
		return DereferenceAdaptor<T>(ptr);
	}

	T **ptr;
};

// Resources can be more abstract and be unsized,
// so we need to have an array of pointers for those cases.
template <typename T>
struct Resource : PointerInterface<T>
{
};

// POD with no unknown sizes, so we can express these as flat arrays.
template <typename T>
struct UniformConstant : Interface<T>
{
};
template <typename T>
struct StageInput : Interface<T>
{
};
template <typename T>
struct StageOutput : Interface<T>
{
};
template <typename T>
struct PushConstant : Interface<T>
{
};
}

struct spirv_cross_shader
{
	struct PPSize
	{
		PPSize()
		    : ptr(0)
		    , size(0)
		{
		}
		void **ptr;
		size_t size;
	};

	struct PPSizeResource
	{
		PPSizeResource()
		    : ptr(0)
		    , size(0)
		    , pre_dereference(false)
		{
		}
		void **ptr;
		size_t size;
		bool pre_dereference;
	};

	PPSizeResource resources[SPIRV_CROSS_NUM_DESCRIPTOR_SETS][SPIRV_CROSS_NUM_DESCRIPTOR_BINDINGS];
	PPSize stage_inputs[SPIRV_CROSS_NUM_STAGE_INPUTS];
	PPSize stage_outputs[SPIRV_CROSS_NUM_STAGE_OUTPUTS];
	PPSize uniform_constants[SPIRV_CROSS_NUM_UNIFORM_CONSTANTS];
	PPSize push_constant;
	PPSize builtins[SPIRV_CROSS_NUM_BUILTINS];

	template <typename U>
	void register_builtin(spirv_cross_builtin builtin, const U &value)
	{
		assert(!builtins[builtin].ptr);

		builtins[builtin].ptr = (void **)&value.ptr;
		builtins[builtin].size = sizeof(*value.ptr) * U::ArraySize;
	}

	void set_builtin(spirv_cross_builtin builtin, void *data, size_t size)
	{
		assert(builtins[builtin].ptr);
		assert(size >= builtins[builtin].size);

		*builtins[builtin].ptr = data;
	}

	template <typename U>
	void register_resource(const internal::Resource<U> &value, unsigned set, unsigned binding)
	{
		assert(set < SPIRV_CROSS_NUM_DESCRIPTOR_SETS);
		assert(binding < SPIRV_CROSS_NUM_DESCRIPTOR_BINDINGS);
		assert(!resources[set][binding].ptr);

		resources[set][binding].ptr = (void **)&value.ptr;
		resources[set][binding].size = internal::Resource<U>::Size;
		resources[set][binding].pre_dereference = internal::Resource<U>::PreDereference;
	}

	template <typename U>
	void register_stage_input(const internal::StageInput<U> &value, unsigned location)
	{
		assert(location < SPIRV_CROSS_NUM_STAGE_INPUTS);
		assert(!stage_inputs[location].ptr);

		stage_inputs[location].ptr = (void **)&value.ptr;
		stage_inputs[location].size = internal::StageInput<U>::Size;
	}

	template <typename U>
	void register_stage_output(const internal::StageOutput<U> &value, unsigned location)
	{
		assert(location < SPIRV_CROSS_NUM_STAGE_OUTPUTS);
		assert(!stage_outputs[location].ptr);

		stage_outputs[location].ptr = (void **)&value.ptr;
		stage_outputs[location].size = internal::StageOutput<U>::Size;
	}

	template <typename U>
	void register_uniform_constant(const internal::UniformConstant<U> &value, unsigned location)
	{
		assert(location < SPIRV_CROSS_NUM_UNIFORM_CONSTANTS);
		assert(!uniform_constants[location].ptr);

		uniform_constants[location].ptr = (void **)&value.ptr;
		uniform_constants[location].size = internal::UniformConstant<U>::Size;
	}

	template <typename U>
	void register_push_constant(const internal::PushConstant<U> &value)
	{
		assert(!push_constant.ptr);

		push_constant.ptr = (void **)&value.ptr;
		push_constant.size = internal::PushConstant<U>::Size;
	}

	void set_stage_input(unsigned location, void *data, size_t size)
	{
		assert(location < SPIRV_CROSS_NUM_STAGE_INPUTS);
		assert(stage_inputs[location].ptr);
		assert(size >= stage_inputs[location].size);

		*stage_inputs[location].ptr = data;
	}

	void set_stage_output(unsigned location, void *data, size_t size)
	{
		assert(location < SPIRV_CROSS_NUM_STAGE_OUTPUTS);
		assert(stage_outputs[location].ptr);
		assert(size >= stage_outputs[location].size);

		*stage_outputs[location].ptr = data;
	}

	void set_uniform_constant(unsigned location, void *data, size_t size)
	{
		assert(location < SPIRV_CROSS_NUM_UNIFORM_CONSTANTS);
		assert(uniform_constants[location].ptr);
		assert(size >= uniform_constants[location].size);

		*uniform_constants[location].ptr = data;
	}

	void set_push_constant(void *data, size_t size)
	{
		assert(push_constant.ptr);
		assert(size >= push_constant.size);

		*push_constant.ptr = data;
	}

	void set_resource(unsigned set, unsigned binding, void **data, size_t size)
	{
		assert(set < SPIRV_CROSS_NUM_DESCRIPTOR_SETS);
		assert(binding < SPIRV_CROSS_NUM_DESCRIPTOR_BINDINGS);
		assert(resources[set][binding].ptr);
		assert(size >= resources[set][binding].size);

		// We're using the regular PointerInterface, dereference ahead of time.
		if (resources[set][binding].pre_dereference)
			*resources[set][binding].ptr = *data;
		else
			*resources[set][binding].ptr = data;
	}
};

namespace spirv_cross
{
template <typename T>
struct BaseShader : spirv_cross_shader
{
	void invoke()
	{
		static_cast<T *>(this)->main();
	}
};

struct FragmentResources
{
	internal::StageOutput<glm::vec4> gl_FragCoord;
	void init(spirv_cross_shader &s)
	{
		s.register_builtin(SPIRV_CROSS_BUILTIN_FRAG_COORD, gl_FragCoord);
	}
#define gl_FragCoord __res->gl_FragCoord.get()
};

template <typename T, typename Res>
struct FragmentShader : BaseShader<FragmentShader<T, Res>>
{
	inline void main()
	{
		impl.main();
	}

	FragmentShader()
	{
		resources.init(*this);
		impl.__res = &resources;
	}

	T impl;
	Res resources;
};

struct VertexResources
{
	internal::StageOutput<glm::vec4> gl_Position;
	void init(spirv_cross_shader &s)
	{
		s.register_builtin(SPIRV_CROSS_BUILTIN_POSITION, gl_Position);
	}
#define gl_Position __res->gl_Position.get()
};

template <typename T, typename Res>
struct VertexShader : BaseShader<VertexShader<T, Res>>
{
	inline void main()
	{
		impl.main();
	}

	VertexShader()
	{
		resources.init(*this);
		impl.__res = &resources;
	}

	T impl;
	Res resources;
};

struct TessEvaluationResources
{
	inline void init(spirv_cross_shader &)
	{
	}
};

template <typename T, typename Res>
struct TessEvaluationShader : BaseShader<TessEvaluationShader<T, Res>>
{
	inline void main()
	{
		impl.main();
	}

	TessEvaluationShader()
	{
		resources.init(*this);
		impl.__res = &resources;
	}

	T impl;
	Res resources;
};

struct TessControlResources
{
	inline void init(spirv_cross_shader &)
	{
	}
};

template <typename T, typename Res>
struct TessControlShader : BaseShader<TessControlShader<T, Res>>
{
	inline void main()
	{
		impl.main();
	}

	TessControlShader()
	{
		resources.init(*this);
		impl.__res = &resources;
	}

	T impl;
	Res resources;
};

struct GeometryResources
{
	inline void init(spirv_cross_shader &)
	{
	}
};

template <typename T, typename Res>
struct GeometryShader : BaseShader<GeometryShader<T, Res>>
{
	inline void main()
	{
		impl.main();
	}

	GeometryShader()
	{
		resources.init(*this);
		impl.__res = &resources;
	}

	T impl;
	Res resources;
};

struct ComputeResources
{
	internal::StageInput<glm::uvec3> gl_WorkGroupID__;
	internal::StageInput<glm::uvec3> gl_NumWorkGroups__;
	void init(spirv_cross_shader &s)
	{
		s.register_builtin(SPIRV_CROSS_BUILTIN_WORK_GROUP_ID, gl_WorkGroupID__);
		s.register_builtin(SPIRV_CROSS_BUILTIN_NUM_WORK_GROUPS, gl_NumWorkGroups__);
	}
#define gl_WorkGroupID __res->gl_WorkGroupID__.get()
#define gl_NumWorkGroups __res->gl_NumWorkGroups.get()

	Barrier barrier__;
#define barrier() __res->barrier__.wait()
};

struct ComputePrivateResources
{
	uint32_t gl_LocalInvocationIndex__;
#define gl_LocalInvocationIndex __priv_res.gl_LocalInvocationIndex__
	glm::uvec3 gl_LocalInvocationID__;
#define gl_LocalInvocationID __priv_res.gl_LocalInvocationID__
	glm::uvec3 gl_GlobalInvocationID__;
#define gl_GlobalInvocationID __priv_res.gl_GlobalInvocationID__
};

template <typename T, typename Res, unsigned WorkGroupX, unsigned WorkGroupY, unsigned WorkGroupZ>
struct ComputeShader : BaseShader<ComputeShader<T, Res, WorkGroupX, WorkGroupY, WorkGroupZ>>
{
	inline void main()
	{
		resources.barrier__.reset_counter();

		for (unsigned z = 0; z < WorkGroupZ; z++)
			for (unsigned y = 0; y < WorkGroupY; y++)
				for (unsigned x = 0; x < WorkGroupX; x++)
					impl[z][y][x].__priv_res.gl_GlobalInvocationID__ =
					    glm::uvec3(WorkGroupX, WorkGroupY, WorkGroupZ) * resources.gl_WorkGroupID__.get() +
					    glm::uvec3(x, y, z);

		group.run();
		group.wait();
	}

	ComputeShader()
	    : group(&impl[0][0][0])
	{
		resources.init(*this);
		resources.barrier__.set_release_divisor(WorkGroupX * WorkGroupY * WorkGroupZ);

		unsigned i = 0;
		for (unsigned z = 0; z < WorkGroupZ; z++)
		{
			for (unsigned y = 0; y < WorkGroupY; y++)
			{
				for (unsigned x = 0; x < WorkGroupX; x++)
				{
					impl[z][y][x].__priv_res.gl_LocalInvocationID__ = glm::uvec3(x, y, z);
					impl[z][y][x].__priv_res.gl_LocalInvocationIndex__ = i++;
					impl[z][y][x].__res = &resources;
				}
			}
		}
	}

	T impl[WorkGroupZ][WorkGroupY][WorkGroupX];
	ThreadGroup<T, WorkGroupX * WorkGroupY * WorkGroupZ> group;
	Res resources;
};

inline void memoryBarrierShared()
{
	Barrier::memoryBarrier();
}
inline void memoryBarrier()
{
	Barrier::memoryBarrier();
}
// TODO: Rest of the barriers.

// Atomics
template <typename T>
inline T atomicAdd(T &v, T a)
{
	static_assert(sizeof(std::atomic<T>) == sizeof(T), "Cannot cast properly to std::atomic<T>.");

	// We need explicit memory barriers in GLSL to enfore any ordering.
	// FIXME: Can we really cast this? There is no other way I think ...
	return std::atomic_fetch_add_explicit(reinterpret_cast<std::atomic<T> *>(&v), a, std::memory_order_relaxed);
}
}

void spirv_cross_set_stage_input(spirv_cross_shader_t *shader, unsigned location, void *data, size_t size)
{
	shader->set_stage_input(location, data, size);
}

void spirv_cross_set_stage_output(spirv_cross_shader_t *shader, unsigned location, void *data, size_t size)
{
	shader->set_stage_output(location, data, size);
}

void spirv_cross_set_uniform_constant(spirv_cross_shader_t *shader, unsigned location, void *data, size_t size)
{
	shader->set_uniform_constant(location, data, size);
}

void spirv_cross_set_resource(spirv_cross_shader_t *shader, unsigned set, unsigned binding, void **data, size_t size)
{
	shader->set_resource(set, binding, data, size);
}

void spirv_cross_set_push_constant(spirv_cross_shader_t *shader, void *data, size_t size)
{
	shader->set_push_constant(data, size);
}

void spirv_cross_set_builtin(spirv_cross_shader_t *shader, spirv_cross_builtin builtin, void *data, size_t size)
{
	shader->set_builtin(builtin, data, size);
}

#endif
