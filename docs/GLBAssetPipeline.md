# Rigged Character Import Pipeline

This is the authoring flow we currently use for cows and any other skinned characters.  Share it with artists so exported GLBs land in-engine with working materials, skeletons, and prefabs.

1. **DCC Export**
   - Export a `.glb` with skinned meshes only (triangulated, max 4 bone weights per vertex).
   - Include the skeleton hierarchy, animation clips (if any), and embed or reference the diffuse textures (`png/jpg` preferred).  Keep names ASCII-only; long names become GUID hints.
   - Apply a consistent base name (e.g. `Cow`) since the importer uses it for asset filenames and GUID hashing.

2. **Run the Importer**
   - From the build output directory:
     ```
     cd build/Debug
     AssimpImporter.exe ..\..\apps\GotMilked\assets\models\cow\Cow.glb
     ```
   - Outputs land next to the GLB:
     - `Cow.gmskel` / `Cow.gmskin`
     - `Cow_<clip>.gmanim` per animation channel
     - `Cow_mat*.mat` material JSON plus associated textures
     - `Cow.animset.json` manifest listing every GUID and source file
     - `Cow.json` prefab with `SkinnedMeshComponent`, `AnimatorComponent`, and default layer wiring

3. **Register Resources**
   - `GameResources` scans `apps/GotMilked/assets/models/**` for `.animset.json` files on load.  The manifest registers all skinned meshes, skeletons, clips, materials, and textures with consistent GUIDs (no manual editing needed).
   - Material JSON supports:
     ```json
     {
       "name": "cow_mat0",
       "shader": "shader::simple_skinned",
       "diffuseColor": [1.0, 1.0, 1.0],
       "diffuseTexture": "cow_mat0_diffuse.png"
     }
     ```
     Use `shader::simple_skinned` for anything that needs palette skinning; the importer sets this automatically.

4. **Prefab Lifecycle**
   - The generated prefab lives in `assets/prefabs`.  You can load it via the Prefab Browser, duplicate it, or wire it into scenes.
   - `SceneResourceController` resolves GUIDs at runtime so hot-reloading works—after re-importing a GLB, hit “Apply Resources” in the debug menu to push the latest meshes/materials into the scene.

5. **Validation**
   - The inspector now shows mesh/shader/material/texture assignments for skinned meshes; you can reassign GUIDs via dropdowns if needed.
   - Prefab Browser lists all mesh components inside the prefab and warns when a mesh is missing a material assignment.
   - Tests (`AnimationPipelineTests`) cover material/shader loading and prefab instantiation to prevent regressions.

### Naming & Texture Guidelines
- Stick to lowercase + underscores for generated aliases (`cow_mat0`, `shader::simple_skinned`).
- Keep diffuse textures in the same folder as the GLB so the importer can copy or decode them.
- For multi-material characters, each material gets a numbered suffix (`mat0`, `mat1`, …) to keep GUIDs deterministic.

Following this checklist ensures GLBs go from DCC → runtime with zero manual edits and stay hot-reload friendly.

