# Free asset sources

Where to find free assets for a HedgehogEngine project, by kind, with what each source's licence
asks of a game that ships its assets and which of its formats the engine reads today (formats:
`Docs/Assets.md` section 2). Researched 2026-10-25.

**Licences change, and on a mixed site they differ per asset.** Read the licence on the asset's own
page before using it, keep a link to it, and record what you ship (see the end of this file).

Licence shorthand:

- **CC0**: public domain; no attribution needed (it is still kind to credit).
- **CC-BY**: free, also commercially, if the author is credited; CC-BY-SA also requires sharing
  derivatives under the same licence.
- **CC-BY-NC**: not for commercial use; avoid in anything that may be sold.
- **Custom**: the site's own terms; summarized here, read in full.

## Models

| Source | Licence | Formats | Fit with the engine |
|--------|---------|---------|---------------------|
| [Poly Haven](https://polyhaven.com/models) | CC0 | glTF, FBX, Blend, USD | glTF loads directly (FeatureTest's HDRIs come from Poly Haven) |
| [Kenney](https://kenney.nl/assets) | CC0 | FBX, OBJ, glTF/GLB | OBJ and GLB load directly; low-poly packs of every genre |
| [Quaternius](https://quaternius.com) | CC0 (most packs; a few mirrors are tagged CC-BY, so check the pack's page) | FBX, OBJ, glTF, Blend | glTF and OBJ load; many characters are rigged and animated |
| [Khronos glTF Sample Assets](https://github.com/KhronosGroup/glTF-Sample-Assets) | Per model (CC0, CC-BY 4.0 and a few with trademark terms), in each model's README and `metadata.json` | glTF, GLB | The reference for glTF features. Some originals are non-commercial: FeatureTest's DamagedHelmet is a CC-BY 4.0 rebuild of a CC-BY-NC 4.0 model, so it is for testing only (`THIRD_PARTY_NOTICES.md`) |
| [Sketchfab](https://sketchfab.com/features/free-3d-models) | Per model (CC0, CC-BY, CC-BY-SA, CC-BY-NC ...), shown on each download | glTF/GLB (Sketchfab's conversion) and the uploader's originals | glTF loads; Sketchfab's exports often use `KHR_texture_transform` and Draco, which the engine ignores (section 2.2) |
| [OpenGameArt](https://opengameart.org) | Per asset (CC0, CC-BY, CC-BY-SA, OGA-BY, GPL ...) | OBJ, FBX, Blend, glTF | Quality varies widely; filter by licence |
| [Fab](https://www.fab.com) (Epic) | Fab's Standard License for its free assets, which reports describe as allowing commercial use in any engine; read it before use | FBX, OBJ, glTF, Unreal packages | Download the FBX or glTF, not the Unreal package; FBX needs the FBX importer (section 2.2) |

## PBR textures and materials

| Source | Licence | Formats | Fit with the engine |
|--------|---------|---------|---------------------|
| [ambientCG](https://ambientcg.com) | CC0 | JPG and PNG maps (1K to 8K), EXR displacement | Colour, normal (pick the OpenGL normal map), roughness and AO load; metalness and roughness need packing into one map (G roughness, B metallic) |
| [Poly Haven textures](https://polyhaven.com/textures) | CC0 | JPG, PNG, EXR | As ambientCG; its glTF download comes with the maps packed |
| [ShareTextures](https://www.sharetextures.com) | CC0 | JPG, PNG | As ambientCG |
| [cgbookcase](https://www.cgbookcase.com) | CC0 | PNG | As ambientCG |
| [3DTextures.me](https://3dtextures.me) | CC0 | JPG, PNG | As ambientCG |

## HDRIs and skies

| Source | Licence | Formats | Fit with the engine |
|--------|---------|---------|---------------------|
| [Poly Haven HDRIs](https://polyhaven.com/hdris) | CC0 | HDR, EXR | `.hdr` loads as an environment (FeatureTest's five skies); 1K or 2K is enough for lighting |
| [ambientCG HDRIs](https://ambientcg.com/list?type=HDRI) | CC0 | HDR, EXR | As Poly Haven |
| [OpenHDRI](https://www.openhdri.org) | Listed as CC0 by third-party guides; read the site's terms | HDR, EXR | `.hdr` loads |
| [HDRMaps freebies](https://hdrmaps.com/freebies) | Custom: HDRMaps' royalty-free licence for its free maps; read it before shipping | HDR, EXR | `.hdr` loads; the free set is a small part of a paid library |

## Characters and animation

| Source | Licence | Formats | Fit with the engine |
|--------|---------|---------|---------------------|
| [Mixamo](https://www.mixamo.com) (Adobe, needs an account) | Custom: free and royalty-free inside a game; the raw character and animation files may not be redistributed or sold as assets (so never commit them to a public repository) | FBX | Needs the FBX importer (section 2.3); convert to glTF in Blender meanwhile |
| [Quaternius](https://quaternius.com) | CC0 | FBX, glTF | Rigged and animated low-poly characters; the glTF versions load with their clips |
| [Kenney](https://kenney.nl/assets) animated characters | CC0 | FBX, GLB | GLB loads with its clips |
| [CMU Graphics Lab Motion Capture Database](http://mocap.cs.cmu.edu) | Custom: free for research and commercial use, with an acknowledgment asked for | ASF/AMC; community BVH conversions | Needs a BVH reader and retargeting (sections 1.3 and 2.3) |

## Sound effects and music

| Source | Licence | Formats | Fit with the engine |
|--------|---------|---------|---------------------|
| [Kenney audio](https://kenney.nl/assets?q=audio) | CC0 | OGG | OGG is not read yet (section 2.4): convert to WAV or FLAC |
| [freesound](https://freesound.org) | Per sound (CC0, CC-BY 4.0, CC-BY-NC); filter to CC0 or CC-BY | WAV, FLAC, OGG, MP3 | WAV, FLAC and MP3 load |
| [Sonniss GameAudioGDC bundles](https://sonniss.com/gameaudiogdc) | Custom: royalty-free commercial use, no attribution; not for training AI models, not to be resold as sounds | WAV | Loads directly; tens of gigabytes of professional effects |
| [OpenGameArt audio](https://opengameart.org/art-search-advanced?field_art_type_tid%5B%5D=13) | Per asset | WAV, OGG, MP3, FLAC | As freesound |
| [Pixabay music and sound effects](https://pixabay.com/music) | Pixabay Content License: commercial use without attribution, not resold as standalone files | MP3 | Loads |
| [Incompetech](https://incompetech.com/music) (Kevin MacLeod) | CC-BY 4.0 | MP3 | Loads; credit "Kevin MacLeod (incompetech.com)" |

## Fonts

| Source | Licence | Formats | Fit with the engine |
|--------|---------|---------|---------------------|
| [Google Fonts](https://fonts.google.com) | SIL Open Font License 1.1 (most), Apache 2.0 or UFL (some) | TTF, OTF | Load directly; Karla, the editor and Hud.yaml's font, comes from here (OFL) |
| [Font Squirrel](https://www.fontsquirrel.com) | Per font, all free for commercial use (OFL, Apache, custom) | TTF, OTF | Load directly |
| [The League of Moveable Type](https://www.theleagueofmoveabletype.com) | SIL OFL 1.1 | TTF, OTF | Load directly |
| [Kenney fonts](https://kenney.nl/assets/kenney-fonts) | CC0 | TTF | Load directly |

Every font is baked for ASCII and Latin-1 only (section 2.5), so pick fonts for those until wider
coverage lands.

## UI and 2D

| Source | Licence | Formats | Fit with the engine |
|--------|---------|---------|---------------------|
| [Kenney UI packs](https://kenney.nl/assets?q=interface) | CC0 | PNG, SVG | PNG loads as a UI image (no sprite atlas yet, so one image per texture) |
| [game-icons.net](https://game-icons.net) | CC-BY 3.0 (credit each icon's author) | SVG, PNG | Export PNG; SVG is not read |
| [Lucide](https://lucide.dev) | ISC | SVG | The editor's own icons, rendered to PNG (`Editor/Resources/Icons/UI/README.md`) |
| [OpenGameArt 2D](https://opengameart.org) | Per asset | PNG | Loads |
| [itch.io free game assets](https://itch.io/game-assets/free) | Per asset (often CC0 or the author's own terms) | PNG, FBX, glTF, OGG, ... | Read each pack's licence text |

## Sample scenes

| Source | Licence | Formats | Fit with the engine |
|--------|---------|---------|---------------------|
| [Khronos glTF Sample Assets](https://github.com/KhronosGroup/glTF-Sample-Assets) (Sponza, Damaged Helmet, Flight Helmet, ABeautifulGame ...) | Per model, as above | glTF, GLB | Sponza and the helmets load; several samples use extensions the engine ignores |
| [McGuire Computer Graphics Archive](https://casual-effects.com/data) | Per model (various; Crytek Sponza CC-BY 3.0) | OBJ, glTF, FBX | OBJ loads (a face without UVs or normals gets defaults since HE-387); large scenes for stress tests |
| [Intel Sponza](https://www.intel.com/content/www/us/en/developer/topic-technology/graphics-research/samples.html) | Free for research use (Intel's own terms); check before shipping | glTF, FBX, USD | glTF loads; a heavier PBR Sponza for benchmarks |
| [Poly Haven](https://polyhaven.com/models) scene sets | CC0 | glTF, Blend | Interiors and props that load as glTF |

## Recording what you ship

Every third-party asset a project ships goes in the engine's `THIRD_PARTY_NOTICES.md` (the
package carries it: `Cooker::LICENCE_FILES`) or, for a game, in the game's own credits file. For
each asset write:

- what it is and where it is in the project (`Assets/Environments/forest_slope_1k.hdr`);
- its author and the page it came from;
- its licence, with a link to the licence text;
- what attribution the licence asks for, and where the game shows it (CC-BY credits belong in
  the game's credits screen too).

Look at the existing Poly Haven, DamagedHelmet and Karla entries for the layout. A sample's licence can be stricter than its source suggests: check every layer (DamagedHelmet's rebuild is CC-BY, its original model CC-BY-NC). Keep CC-BY-NC,
GPL and "personal use only" assets out of the repository: they stop the project from being
shipped or sold.
