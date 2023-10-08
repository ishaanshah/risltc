import cstruct
import struct
import sys
import bpy
import os
import bmesh
import numpy as np
from mathutils import Vector, Matrix
from typing import Any, BinaryIO, Dict, List

class first_person_camera_t(cstruct.MemCStruct):
    __def__ = """
        struct {
            //! The position of the camera in world space
            float position_world_space[3];
            //! The rotation of the camera around the global z-axis in radians
            float rotation_z;
            //! The rotation of the camera around the local x-axis in radians. Without
            //! rotation the camera looks into the negative z-direction.
            float rotation_x;
            //! The vertical field of view (top to bottom) in radians
            float vertical_fov;
            //! The distance of the near plane and the far plane to the camera position
            float near;
            float far;
            //! The default speed of this camera in meters per second when it moves
            //! along a single axis
            float speed;
            //! 1 iff mouse movements are currently used to rotate the camera
            int rotate_camera;
            //! The rotation that the camera would have if the mouse cursor were moved
            //! to coordinate (0, 0) with rotate_camera enabled
            float rotation_x_0;
            float rotation_z_0;
        }
    """

class polygonal_light_info_t(cstruct.MemCStruct):
    __def__ = """
        struct {
            uint32_t legacy_count;
            uint32_t polygonal_light_count;
        }
    """

class polygonal_light_t (cstruct.MemCStruct):
    __def__ = """
        struct {
            float rotation_angles[3];
            float scaling_x;
            float translation[3];
            float scaling_y;
            float radiant_flux[3];
            float inv_scaling_x;
            float surface_radiance[3];
            float inv_scaling_y;
            float plane[4];
            uint32_t vertex_count;
            uint32_t texturing_technique; // Use 32 bit int instead of enum
            uint64_t path_size;
        }
    """

    def __init__(self, buffer: bytes | BinaryIO | None = None, flexible_array_length: int | None = None, **kargs: Dict[str, Any]) -> None:
        self.texture_file_path: str = None
        self.vertices_plane_space: List[float] = []
        super().__init__(buffer, flexible_array_length, **kargs)


def read_polygonal_light(f) -> polygonal_light_t:
    light = polygonal_light_t(f)
    
    # Read texture path
    if light.path_size:
        light.texture_file_path = f.read(light.path_size)[:-1].decode("ascii")
    
    # Read two pointers (i.e 16 bytes on 64 bit)
    f.read(16)
    
    # Read vertices (4 bytes per vertex)
    light.vertices_plane_space = list(struct.unpack("f"*light.vertex_count*4, f.read(4*4*light.vertex_count)))

    return light

def write_polygonal_light(light: polygonal_light_t, f):
    f.write(light.pack())
    
    # Write texture path
    if light.path_size:
        f.write(light.texture_file_path.encode("ascii")+bytes(1))
    
    # Write two pointers (i.e 16 bytes on 64 bit machine)
    f.write(struct.pack("Q", 0))
    f.write(struct.pack("Q", 0))
    
    # Write vertices (4x vertices, 4 bytes per vertex)
    for i in range(light.vertex_count*4):
        f.write(struct.pack("f", light.vertices_plane_space[i]))

def read_quicksave(f):
    camera = first_person_camera_t(f)
    light_info = polygonal_light_info_t(f)
    lights = []
    for _ in range(light_info.polygonal_light_count):
        light = read_polygonal_light(f)
        lights.append(light)

    return camera, light_info, lights

def write_quicksave(camera, light_info, lights, f):
    f.write(camera.pack())
    f.write(light_info.pack())
    for i in range(light_info.polygonal_light_count):
        write_polygonal_light(lights[i], f)

with open("data/quicksaves/attic.save", "rb") as f:
    camera, light_info, lights = read_quicksave(f)

quicksave_dir = os.path.join(bpy.path.abspath("//"), "data", "quicksaves")
with open(os.path.join(quicksave_dir, "Bistro_outside.save"), "rb") as f:
    camera, light_info, lights = read_quicksave(f)

collection_name = "lights"
collection: bpy.types.Collection = bpy.data.collections[collection_name]

lights = []
for obj in collection.all_objects:
    bm = bmesh.new()
    bm.from_mesh(obj.to_mesh())
    
    # Get radiance
    _, world_rot, _ = obj.matrix_world.decompose()
    emissive_color = obj.material_slots[0].material.node_tree.nodes['Principled BSDF'].inputs['Emission'].default_value
    emissive_strength = obj.material_slots[0].material.node_tree.nodes['Principled BSDF'].inputs['Emission Strength'].default_value
    radiance = np.asarray([ emissive_color[0], emissive_color[1], emissive_color[2]]) * emissive_strength
    for f in bm.faces:
        light = polygonal_light_t()
        light.vertex_count = len(f.verts)
        
        # Scaling
        light.scaling_x = 1
        light.scaling_y = 1
        
        # Emissive stuff
        area = f.calc_area()
        # light.radiant_flux = (radiance * area * pi).tolist()   # This is reversed in update_polygon
        light.radiant_flux = radiance.tolist()   # This is reversed in update_polygon
        
        # Location
        orig = obj.matrix_world @ f.verts[0].co
        light.translation = [orig.x, orig.y, orig.z]
        
        # Rotation
        # Define plane co-ordinate axis
        normal: Vector = (world_rot @ f.normal).normalized()
        plane_x = ((obj.matrix_world @ f.verts[1].co) - orig).normalized()
        plane_y = normal.cross(plane_x)

        rotation_mat = Matrix((plane_x, plane_y, normal))
        rotation_hack = rotation_mat.copy()
        rotation_hack[0][1] *= -1
        rotation_hack[1][0] *= -1
        rotation_hack[1][2] *= -1
        rotation_hack[2][1] *= -1
        rotation = rotation_hack.to_euler("XYZ")
        light.rotation_angles = [rotation.x, rotation.y, rotation.z]

        # light.vertices_plane_space = [0, 0, 0, 0] + [1, 0, 0, 0] + [0, 1, 0, 0]
        for v in f.verts:
            global_co = obj.matrix_world @ v.co
            plane_co = global_co - orig
            plane_co = rotation_mat @ plane_co
            light.vertices_plane_space.append(plane_co.x)
            light.vertices_plane_space.append(plane_co.y)
            light.vertices_plane_space.append(0)
            light.vertices_plane_space.append(0)
        
        lights.append(light)
    bm.free()

with open(os.path.join(quicksave_dir, "ZeroDay.save"), "rb") as f:
    camera, light_info, _ = read_quicksave(f)
    light_info.polygonal_light_count = len(lights)

with open(os.path.join(quicksave_dir, "ZeroDay.save"), "wb") as f:
    write_quicksave(camera, light_info, lights, f)