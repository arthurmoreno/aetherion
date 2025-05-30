.. Aetherion documentation master file, created by sphinx-quickstart on Sat May 10 20:33:31 2025.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

Aetherion: The Cosmic Engine
============================

.. image:: _static/aetherion-logo-128.png
   :align: center
   :alt: Aetherion Logo
   :width: 200 px

Welcome to the official documentation for **Aetherion**, a game engine designed to bridge the gap between classic and modern sandbox simulations. Inspired by titles like *Tibia*, *Terraria*, *Minecraft*, *Dwarf Fortress*, and *Valheim*, Aetherion offers a unique blend of 2D physics and deep world simulation.

.. image:: _static/aetherion-logo.png
   :align: center
   :alt: Aetherion Logo

Overview
--------

<Put an example of the Aetherion engine in action here, such as a screenshot or a video link.>

Aetherion is built upon the principles of:

- **Tibia-like Physics**: Emulating the classic 2D physics system, providing a familiar yet enhanced experience.
- **Advanced World Simulation**: Incorporating features like dynamic water physics and environmental interactions, akin to *Dwarf Fortress*.
- **Entity-Component-System Architecture**: Leveraging the power of EnTT for efficient and flexible game object management.
- **Modular Rendering**: While defaulting to a 2D top-down perspective, Aetherion supports alternative rendering strategies, including 3D views.:contentReference[oaicite:16]{index=16}

Contents
--------

.. toctree::
   :maxdepth: 2
   :caption: Home

   index

.. toctree::
   :maxdepth: 2
   :caption: Documentation

   
   topics/getting_started/index
   topics/installation/index
   physics_simulation
   world_simulation
   ecs_architecture
   rendering_options
   api_reference
   contributing
   faq

Indices and Tables
------------------

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`
