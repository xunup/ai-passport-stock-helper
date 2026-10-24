<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Assets

This directory stores reusable fonts, images, music, and sound effects, organized by asset type.

Keep each asset in the matching subdirectory and document its destination, naming, integration method, and source/license. Do not mix binary assets with Markdown documentation.

## Fonts

Store reusable font files and generated font sources in `fonts/`.

- Use descriptive names that include the family, weight, size, and format when relevant.
- Document the source, license, character range, conversion command, and expected destination.
- Check Flash and internal-RAM impact before adding a font; the ESP32-C3 has no PSRAM.
- Do not commit fonts whose license does not permit redistribution.

| File | Format and coverage | Use, source, and license |
| --- | --- | --- |
| [`fonts/lv_font_cn_16.c`](fonts/lv_font_cn_16.c) | LVGL 9 C source, 16 px, 2 bpp; ASCII, GB2312 level-1 Hanzi, and 23 UI punctuation marks (3,873 codepoints, including 3,778 CJK glyphs) | Chinese UI font for the stock ticker application, compiled by `main/CMakeLists.txt`. Generated from Source Han Sans SC Regular, Copyright 2014-2025 Adobe, under the SIL Open Font License 1.1; the license text is in [`fonts/LICENSE-OFL.txt`](fonts/LICENSE-OFL.txt). Regenerate with `python3 tools/gen_cjk_font.py --font <SourceHanSansSC-Regular.otf>`; output is written to `assets/fonts/`. Glyph data is `const` and linked into flash `.rodata`, so it does not consume DRAM. |
| [`fonts/lv_font_cn_22.c`](fonts/lv_font_cn_22.c) | LVGL 9 C source, 22 px, 2 bpp; same 3,873 codepoints and 3,778 CJK glyphs | Larger companion font for quote prices and headings, with the same source, license, generation command, integration, and flash placement as the 16 px font. |

The generator intentionally limits Hanzi to GB2312 level one so the two fonts fit the 8 MB flash budget. In the current stock ticker firmware the two fonts occupy about 768 KB of flash (16 px: 279 KB; 22 px: 489 KB), entirely in `.rodata`, with no DRAM cost. A dynamic stock name containing a level-two character is replaced with `?` on the device; the full name remains available on the configuration page.

## Images

Store reusable source images and generated display assets in `images/`.

| File | Dimensions and format | Use and source |
| --- | --- | --- |
| [`images/home.jpg`](images/home.jpg) | 3840 × 2160, JPEG | Product hero image embedded in both project README files to foreground AI Passport and its open, maker-oriented identity. |
| [`images/readme-hardware-specs.png`](images/readme-hardware-specs.png) | 2172 × 724, PNG RGBA | Optional technical infographic retained as a reference asset; it is no longer used as the homepage hero. Generated for this repository with the built-in image generation tool on 2026-09-17; the six labels and values were checked against the documented hardware contract. |
| [`images/logo-wordmark.png`](images/logo-wordmark.png) | 1648 × 336, PNG RGBA | Transparent black wordmark extracted from the repository's original `images/logo.png`; embedded in both project README files for light backgrounds. |
| [`images/logo-wordmark-dark.png`](images/logo-wordmark-dark.png) | 1648 × 336, PNG RGBA | White version of the extracted wordmark, used by the README `<picture>` element when GitHub is in dark mode. |

- Use descriptive names and document dimensions, pixel format, conversion steps, and destination.
- Prefer formats suitable for the 240 × 320 RGB565 display and account for Flash and internal RAM.
- Preserve editable sources where licensing permits, and record the source and license.
- Never commit device QR secrets, credentials, or personal data in images.

## Music and sound effects

Store reusable music and sound-effect sources in `music/`.

- Document the source, license, sample rate, bit depth, channels, conversion command, and destination.
- Prefer 16 kHz, 16-bit mono PCM when it matches the current BSP audio path.
- Check Flash and internal-RAM cost before embedding audio; stream or chunk long recordings.
- Do not commit media without redistribution permission.
