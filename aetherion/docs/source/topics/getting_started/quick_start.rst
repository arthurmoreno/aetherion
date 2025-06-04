.. _quick_start:

Quick Start
===========

This short walkthrough shows how to create a world, add an entity and query the voxel grid from Python.
It assumes that the :ref:`installation` steps have been completed and that ``lifesimcore`` is importable.

.. code-block:: python

   import lifesimcore

   # Create a world with 100x100x10 voxels
   world = lifesimcore.World(100, 100, 10)
   world.initialize_voxel_grid()

   # Create a basic entity using the helper interface
   entity = lifesimcore.EntityInterface(
       lifesimcore.Position(1, 2, 0),
       lifesimcore.Velocity(0.0, 0.0, 0.0),
   )
   entity_id = world.create_entity(entity)

   # Store entity id in the voxel grid
   data = lifesimcore.GridData(terrainID=0, entityID=int(entity_id), eventID=0, lightingLevel=0.0)
   world.set_voxel(1, 2, 0, data)

   # Retrieve the voxel contents
   result = world.get_voxel(1, 2, 0)
   print(result.entityID)

The C++ ``World`` object can be passed to other modules (such as a renderer) using ``World.get_ptr`` which returns a capsule pointer.
