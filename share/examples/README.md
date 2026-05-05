# Examples Maintenance Guide

This directory contains the canonical source for PreVABS example cases
and their user-facing example documentation.

Each example owns:

- its runnable input files
- its example-specific images
- its example narrative in `README.md`
- its metadata in `meta.json`

The user manual example pages under
`doc/user/source/contents/examples/` are generated wrappers. They should
not be maintained by hand unless you are changing the wrapper format
itself.

## Directory layout

Each example lives in its own directory:

```text
share/examples/ex_some_case/
  README.md
  meta.json
  <input files>
  <example images>
  <tracked outputs, if intentionally kept>
```

Example directories should contain only files that belong to that case.
If a file is needed by the example documentation or is intentionally
kept as part of the example, declare it in `meta.json`.

## `meta.json`

`meta.json` is the single source of truth for example metadata.

Required documentation fields:

- `slug`
- `label`
- `title`
- `card_title`

Common ordering and summary fields:

- `summary`
- `order`

Tracked file lists:

- `inputs`
- `outputs`
- `doc_images`

Documentation rendering fields:

- `preview`
- `preview_caption`
- `preview_width`
- `run_command`

Example:

```json
{
  "slug": "example_box",
  "label": "example-box-beam",
  "title": "Box beam",
  "card_title": "Box beam",
  "summary": "Closed box-beam example defined directly in the XML input.",
  "order": 20,
  "inputs": [
    "box_cus.xml"
  ],
  "outputs": [
    "box_cus.png"
  ],
  "doc_images": [
    "examplebox0.png"
  ],
  "preview": "box_cus.png",
  "preview_caption": "Cross-section viewed in Gmsh."
}
```

### Meaning of tracked file lists

- `inputs`: files that are part of the runnable example case
- `outputs`: files intentionally kept as reference outputs
- `doc_images`: images used by the example `README.md`

These fields are reused by:

- the example page generator
- the Sphinx staging step
- the example cleanup script

If a file should survive cleanup or appear in the built documentation,
declare it in `meta.json`.

## Writing `README.md`

Each example `README.md` is written in MyST Markdown and is included into
the user manual.

Guidelines:

- Keep paths local to the example directory.
- Put example-owned images in the same directory as the example.
- Use relative links such as `[case.xml](case.xml)`.
- Use local figure targets such as ```` ```{figure} some_image.png ````.
- Keep the first line as a short summary sentence when practical.

Recommended section structure:

- one-line summary
- `## Overview`
- `## Input`
- `## Run the example`
- `## Output`
- result or discussion sections as needed

Use [ex_box/README.md](./ex_box/README.md) as the reference pattern.

## Adding a new example

1. Create a new directory under `share/examples/`, for example
   `ex_new_case/`.
2. Add the runnable input files.
3. Add any figures used by the example documentation into the same
   directory.
4. Write `README.md` in MyST Markdown.
5. Create `meta.json` and declare:
   - page metadata
   - `inputs`
   - `outputs`
   - `doc_images`
   - `preview`
6. Regenerate the wrapper pages:

```text
.venv\Scripts\python.exe doc\user\tools\generate_examples_docs.py
```

7. Build the user manual:

```text
.venv\Scripts\python.exe -m sphinx -b html doc\user\source doc\user\_build\html
```

## Updating an existing example

If you only change the example narrative or local files inside one
example directory:

- update `README.md`
- update `meta.json` if tracked files changed
- rebuild the docs

If you change wrapper-level metadata such as `title`, `label`,
`card_title`, `order`, or `preview`, regenerate the wrapper pages
before rebuilding the docs.

## Generated documentation flow

The example documentation build has two stages:

1. `doc/user/tools/generate_examples_docs.py`
   This generates the wrapper pages and examples index.

2. `doc/user/source/conf.py`
   During Sphinx build, it stages only the tracked example files into:
   - `_example_assets/`
   - `_example_includes/`

This staging step keeps Sphinx reads inside the documentation source
tree while allowing the real source of truth to stay in
`share/examples/`.

## Cleaning example directories

Use the cleanup helper to remove temporary files not declared in
`meta.json`:

```text
.venv\Scripts\python.exe tools\clean-example-files.py --filter ex_box
```

To actually delete the removable files:

```text
.venv\Scripts\python.exe tools\clean-example-files.py --filter ex_box --apply
```

The script keeps:

- `README.md`
- `meta.json`
- files listed in `inputs`
- files listed in `outputs`
- files listed in `doc_images`
- the `preview` file

## About `files.json`

`share/examples/files.json` is deprecated. The maintained source of
truth is now the per-example `meta.json` file in each example
directory.
