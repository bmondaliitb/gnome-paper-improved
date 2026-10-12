# Papers enhanced text

The recovered FreeType shim adds a little weight to outline glyphs loaded by
Papers. The wrapper applies its environment only to Papers and its children;
it does not change desktop-wide font settings.

## Requirements

- Native Linux Papers installed at `/usr/bin/papers`. This wrapper does not
  launch Flatpak or Snap installations.
- Bash and the FreeType runtime library.
- To build: GCC, pkg-config, and FreeType development headers.
- GNOME or another desktop that supports `.desktop` launchers.
- If Papers is confined by AppArmor: permission to map the shim, as described
  in the AppArmor section below. This was required on Ubuntu 26.04 with
  Papers 50.2, and the user confirmed rendering works after adding it.

Install the application and build requirements on Ubuntu/Debian:

```bash
sudo apt install papers build-essential pkg-config libfreetype-dev
```

`desktop-file-utils` is optional; when available, the installer uses its
`update-desktop-database` command to refresh the desktop MIME cache.

## Install and choose darkening

Run the installer as your normal user, without `sudo`:

```bash
./install.sh --weight 48
```

The chosen weight applies to both the enhanced GNOME launcher and the
terminal wrapper. Choose a value to suit your display and PDFs:

| Weight | Effect |
| --- | --- |
| 0 | Disable the shim's outline emboldening |
| 20 | Moderate |
| 24 | Default starting point |
| 28–32 | Heavier |
| 48 | Strong darkening |
| 64 | Very strong; useful for checking the effect |

Any integer from 0 to 256 is accepted; high values can obscure small text.
Zero still leaves the wrapper's FreeType stem-darkening properties enabled.

`./install.sh` without `--weight` preserves your saved choice, or uses 24 on
the first installation. It builds a new shim when build tools are available.
Otherwise, it reuses an already-installed binary, if one exists; a fresh
installation requires the build tools.

Change the saved weight later without rebuilding:

```bash
./install.sh --reuse-existing --weight 32
```

The setting is stored as a plain integer in `~/.config/papers-bold/weight`
(or `$XDG_CONFIG_HOME/papers-bold/weight` when set). Reinstallation preserves
it unless `--weight` is supplied. `PDF_FONT_WEIGHT` overrides it for a single
launch:

```bash
PDF_FONT_WEIGHT=64 ~/.local/bin/papers-bold document.pdf
```

Close any open Papers windows, then search GNOME for **Document Viewer —
Enhanced Text**. An already-running Papers process can receive the document
without inheriting the wrapper environment. `DBusActivatable=false` makes
the launcher execute the wrapper, but does not prevent that process reuse.

Or launch from a terminal:

```bash
~/.local/bin/papers-bold document.pdf
PDF_FONT_WEIGHT=28 ~/.local/bin/papers-bold document.pdf
```

Close Papers between comparisons. Weight uses FreeType 26.6 units
(24 is 0.375 pixels for scaled
outlines). Bitmap glyphs and scanned PDF images are unaffected. The shim
emboldens scaled outlines before rasterization, including loads that request
immediate rendering. Unscaled and metrics-only loads are left untouched.
It may affect other FreeType text rendered within Papers.

## Diagnose no visible change

Close Papers, rebuild with `./install.sh`, then run:

```bash
PDF_FONT_WEIGHT=64 ~/.local/bin/papers-bold --debug /path/to/document.pdf
```

Look for `shim loaded`, `glyph hook reached`, and `outline emboldened` in
the terminal. The last message confirms that some outline glyphs changed,
but these may belong to the UI rather than the PDF. Closing Papers prints
glyph counters. A loader error or missing `shim loaded` means the shim did
not load; raising the weight cannot fix that. If only `shim loaded` appears,
the rendering path did not call this hook. The debug wrapper refuses to
launch when it can see an existing Papers process, to avoid reusing it.

### Ubuntu AppArmor denies loading the shim

On this machine, the kernel log confirmed `apparmor="DENIED"`,
`operation="file_mmap"`, `profile="/usr/bin/papers"`, and `denied_mask="m"`
for the installed shim. This prevents all emboldening, regardless of weight.

The fix requires an administrator change outside the user-local installation.
Add this exact rule for this account to `/etc/apparmor.d/local/usr.bin.papers`:

```text
owner /home/bmondal/.local/lib/papers-bold/ft-pdf-bold.so mr,
```

On another account, replace `/home/bmondal` with that user's absolute home
directory. Use `sudoedit /etc/apparmor.d/local/usr.bin.papers` to add the
rule once, preserving any existing rules. Verify that the installed Papers
profile or its abstraction includes this local file before using it on
another distribution.

Then reload the profile:

```bash
sudo apparmor_parser -r /etc/apparmor.d/usr.bin.papers
```

The installed Papers abstraction includes that local file; the exception
therefore applies to the Papers profiles using it, including its helpers.
It grants read and executable mapping access only to this exact owner-held
file. AppArmor stays enabled. The shim remains under `~/.local` and is loaded
by the enhanced wrapper. `install.sh` and `remove.sh` do not edit system
security policy. To undo the exception, remove just the rule above from the
local file and reload the profile with the same command.

After the change, close Papers and retry the `--debug` command above.

To test the emboldening code independently of the desktop session:

```bash
gcc -Wall -Wextra -Werror $(pkg-config --cflags freetype2) \
    test-shim.c ft-pdf-bold.c -o /tmp/test-papers-bold -ldl -lfreetype -pthread
/tmp/test-papers-bold /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf
```

This checks glyph coverage with and without immediate rasterization, weight
zero, monochrome rendering, and unscaled outlines. It does not verify that
Papers loads the shared library; use `--debug` for that.

The normal launcher and default PDF association are unchanged. No global
environment configuration is installed.

## Remove

Run `./remove.sh` to remove the installed wrapper, launcher, shim, and
installed source, saved weight, and refresh the launcher database. Empty installation
directories are removed; any extra files in them are preserved. It is safe
to run again if already uninstalled. The repository retains the source for
reinstalling. Close and reopen Papers to return to normal rendering.
If you added the AppArmor exception, remove that exact rule with `sudoedit`
and reload the profile as described above; `remove.sh` leaves system policy
untouched.
