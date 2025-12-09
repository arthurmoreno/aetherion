Aetherion Engine Overview
=========================

This overview introduces the core C++ concepts that power the Aetherion game engine.

Entities and Components
-----------------------
Entities are lightweight identifiers managed through the `entt` library. The
:cpp:class:`EntityInterface` class wraps an entity's components in a tuple and
uses a bitmask to track which components are present. Components such as
:cpp:class:`EntityTypeComponent`, :cpp:class:`HealthComponent` and others are
stored and retrieved through this interface.

The interface can be serialised to and from bytes so that entities can be easily
sent between C++ and Python or saved to disk.

SDL2 Bridge
-----------
Aetherion renders with SDL2 while exposing rendering control to Python. Functions
such as ``load_texture`` and ``RenderQueue.render`` receive integer parameters
that represent C++ ``SDL_Renderer*`` pointers. When these integers are passed
from Python (e.g., from ``pysdl2``), the C++ side casts them back to the native
pointer type. This approach allows existing Python SDL2 code to drive the engine
without wrapping every SDL2 call.

Voxel Grid
----------
Terrain, entities and events are stored in the :cpp:class:`VoxelGrid`. Each cell
tracks terrain, entity and event identifiers plus lighting information. The grid
can serialise itself using OpenVDB and FlatBuffers, providing both full
precision storage and lightweight views via :cpp:class:`VoxelGridView`.

World
-----
The :cpp:class:`World` class orchestrates game systems. It holds a
:cpp:class:`VoxelGrid`, an EnTT registry for entities, and dispatchers for
physics and ecosystem updates. Python can obtain a capsule pointer to the world
via ``World.get_ptr`` and use it inside rendering or GUI code.

Systems and Events
------------------
Each frame the ``World`` updates a collection of C++ systems including physics, metabolism and combat. Events are delivered through an EnTT dispatcher allowing decoupled systems written in either C++ or Python. Python systems can be registered with ``World.addPythonSystem`` and participate in the same update loop.

Saving and Loading
------------------
Both entities and voxel data can be serialised. ``EntityInterface.serialize`` returns bytes representing the current components while the :cpp:class:`VoxelGrid` provides OpenVDB output for terrain data. These can be stored on disk or transferred over the network.
