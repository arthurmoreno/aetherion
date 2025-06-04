.. _faq:

Frequently Asked Questions
==========================

**Why is there both C++ and Python code?**
   The engine core is implemented in modern C++ for performance while the higher level game logic can be scripted in Python. The ``lifesimcore`` package exposes the C++ classes to Python using nanobind.

**How do I use my own SDL2 renderer?**
   Functions such as ``load_texture`` and ``RenderQueue.render`` accept integer values that are cast back to ``SDL_Renderer*`` on the C++ side. Pass ``int(renderer_ptr)`` from ``pysdl2`` to integrate your existing rendering pipeline.

**Can I run the engine without a GPU?**
   Yes. The default build targets CPU execution only. GPU acceleration may be added in future releases.
