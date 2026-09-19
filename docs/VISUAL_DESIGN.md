# Visual design

[繁體中文](VISUAL_DESIGN.zh-tw.md)

## An editing companion

The README artwork introduces an original fox editing companion guiding illuminated document pages through a night workspace. The moving pages connect the illustration to the plugin's text workflow, while the character directs rather than automatically applies the edits. The fox is a creative direction chosen for this redesign, not an established Notepad++ character or an official endorsement.

The banner uses a wide composition with the character on the left and the document flow extending across the frame. Two companion illustrations echo the same visual language around the workflow and prompt-inspection sections. Deep navy surroundings, warm amber fur, and blue-white document trails keep the series coherent across both README languages. The images contain no embedded project title, so the project name remains selectable, accessible Markdown text.

The artwork was generated with AI in a polished 3D illustration style. Render terminology used during concept development describes the desired appearance; it is not a claim about a particular rendering engine. No third-party logo or application interface was supplied as an image reference.

## Interface presentation

The workspace follows the sequence **configure → compose → inspect → apply**. Connection and prompt options appear before the response; output controls sit next to the input workflow. Original-response access helps users separate readable formatting from the text they may write to a document.

## Screenshot provenance

The three `0.2.0.6` screenshots were newly captured from the candidate's actual Win32 dialog resources and production panel/settings implementation. A standalone documentation fixture hosts those dialogs with in-memory settings and blocked network adapters. It adds a descriptive preview window title and sample conversation content; the controls themselves come from the plugin source.

- Language: English (interface and sample content).
- Source revision: `4c5e27a29d112ed524cf6ac2f9500dff4da3c47e` (candidate, not the published v0.1.0 UI).
- Data: invented editing instructions and an explicitly labeled sample reply; no user documents or credentials.
- Capture: original-resolution PNGs capturing the preview window client areas on the primary monitor. Cursor and Computer Use overlays are excluded; no AI-generated controls or composited application content.
- Scope: interface appearance only. These images do not establish live provider behavior, docking inside Notepad++, editor write safety, or Plugins Admin installation acceptance.

The existing release ZIP and older screenshots are retained. The README now references these new files explicitly, so the artwork and interface evidence remain distinct.
