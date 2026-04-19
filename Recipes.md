# Cursed Image Editor: Advanced Compositing Recipes

Welcome to the **CursedImageEditor** compositing guide

Because this engine features a multi-layer architecture, a full AST math evaluator, channel masking, and 4K-capable separable convolutions, it operates less like a standard paint program and more like a command-line node-graph compositor.

---

## 1. The "Retro 3D" Chromatic Aberration

By isolating the color channels and applying a heavy separable blur *only* to the Red channel, we can simulate the dreamy, out-of-focus "3D glasses" effect heavily used in cyberpunk and synthwave aesthetics.

**The Workflow:**
```text
> load photo.bmp
> new 800 600 l0
> l0 = photo
> select l0
> filter blur 15
> l0 = photo[r] + l0[g,b,a]
> select l0
> save out_retro.png
```

**How it works:** We duplicate the original image into a named layer `l0`, apply a 15-pixel separable Gaussian blur to it, then use the math evaluator's channel masking (`blur_layer[r]`) to composite the blurry red data back over the crisp green, blue, and alpha channels of the original. The `select` commands now target layers by their name rather than index.

---

## 2. True "High-Pass" Sharpening

Multiplying an edge map over an image creates a dark, gritty "grunge" effect. For a professional, photorealistic sharpen that makes textures pop without ruining the overall brightness of the image, we add a subtle high-pass edge map back onto the original.

**The Workflow:**
```text
> load photo.bmp
> new 800 600 edges
> edges = photo
> select edges
> filter edge2
> result = photo + (edges * 0.3)
> select result
> save out_sharp.png
```

**How it works:** The `edge2` (8-connected Laplacian) kernel extracts high-frequency details. We scale that detail map down to 30% (`edges * 0.3`) to prevent blown-out pixels, then linearly add it back to the original colors.
---

## 3. The "Neon Edge" Colorizer

The Sobel filters detect edges based on harsh gradients. By combining the horizontal and vertical passes, we can extract a perfect outline of the subject, then use channel math to kill the red data and boost blue and green — resulting in a glowing cyan neon outline.

**The Workflow:**
```text
> load subject.bmp
> new 800 600 sobel_x
> new 800 600 sobel_y
> sobel_x = l0
> sobel_y = l0
> select sobel_x
> filter sobel_x
> select sobel_y
> filter sobel_y
> l0 = sobel_x + sobel_y
> l0 = l0[r] * 0.0 + l0[g] * 1.5 + l0[b] * 2.0 + l0[a]
> select l0
> save neon.png
```

**How it works:** Since there is no power operator for a true Pythagorean combination, adding the X and Y edge maps provides a highly accurate approximation of the gradient magnitude. The final expression acts as a programmatic color-grading node — zeroing out the red channel and over-saturating green and blue. Layer names now match the filter being applied, which makes multi-layer workflows far easier to read.

---

## 4. Cinematic Bloom (Screen Blending)

Bloom simulates intense light bleeding over the edges of bright objects, mimicking a physical camera lens. This is achieved by creating a heavily blurred, blown-out copy of the image and adding a fraction of that light back on top.

**The Workflow:**
```text
> load render.bmp
> new 800 600 bloom
> bloom = l0
> select bloom
> filter blur 40
> l0 = l0 + (bloom * 0.45)
> select l0
> save dreamy.png
```

**How it works:** A 40-pixel separable blur diffuses the light across the canvas. Adding 45% of that blurred light back to the original mathematically simulates a "Screen" or "Linear Dodge" blend mode, raising the black levels and making highlights glow softly.
---

## 5. Geometric Emboss Masking

You can combine the built-in rasterization tools with the math evaluator to create dynamic alpha masks. This allows you to restrict heavy convolutions — like an Emboss filter — to specific geometric areas, simulating stamped metal or frosted glass.

**The Workflow:**
```text
> load l0.bmp
> new 800 600 emboss
> new 800 600 mask
> emboss = l0
> select emboss
> filter emboss

> select mask
> color 255 255 255 255
> draw fillcircle 400 300 150
> filter blur 10

> l0 = (emboss * mask[r]) + (l0 * (1.0 - mask[r]))
> select l0
> save stamped_coin.png
```

**How it works:**

1. `emboss` holds a fully embossed version of the texture.
2. `mask` holds a white-filled circle, blurred by 10 pixels to create a soft anti-aliased edge.
3. The final equation is a true **Lerp (Linear Interpolation)**. Where the mask is pure white (`1.0`), it outputs 100% of the embossed layer. Where the mask is black (`0.0`), it outputs 100% of the original. The blurred edges blend seamlessly between the two.

---

## 6. B&W Noir with Soft Glow

Combine the `greyscale` filter with a ghost-glow bloom to produce a classic noir aesthetic — sharp monochrome detail with a dreamy luminous haze over the highlights.

**The Workflow:**
```text
> load l0.bmp
> filter greyscale

> new 800 600 glow
> glow = l0
> select glow
> filter blur 25

> l0 = l0 + (glow * 0.2)
> select l0
> save noir_glow.png
```

**How it works:** First, `filter greyscale` converts the image to monochrome in-place using ITU-R BT.601 luma coefficients (`Y = 0.299·R + 0.587·G + 0.114·B`), preserving perceptual luminosity. Then a 25-pixel bloom layer adds back 20% soft-light, simulating the classic "vaseline on the lens" technique used in 1940s cinema. Because the source is already greyscale, the bloom stays neutral and doesn't reintroduce color.

---

## 7.Triangle Spotlight Mask

The `filltriangle` primitive lets you define arbitrary directional spotlights. By blurring the triangle's edges and using it as a Lerp mask, you can focus the viewer's attention on a wedge-shaped area of any image.

**The Workflow:**
```text
> load l0.bmp
> new 800 600 darkened
> new 800 600 spotlight

> darkened = l0 * 0.15

> select spotlight
> color 255 255 255 255
> draw filltriangle 400 50 750 600 50 600
> filter blur 20

> l0 = (l0 * spotlight[r]) + (darkened * (1.0 - spotlight[r]))
> select l0
> save spotlight_triangle.png
```

**How it works:**

1. `darkened` is a very dim copy of the original (15% brightness), used as the "shadow" region.
2. `spotlight` is a white filled triangle defined by three points: apex `(400, 50)` and base corners `(750, 600)` and `(50, 600)`, forming a downward-pointing cone. The edges are then blurred for a smooth falloff.
3. The final Lerp composites full-brightness original in the lit region and the darkened layer in the shadows, with a feathered boundary in between.

---

## 8. Multi-Layer Blend & Difference Map

The math evaluator supports full arithmetic over multiple layers simultaneously. This recipe demonstrates a 70/30 dissolve blend and a difference-map inspection workflow — useful for comparing processing stages.

**The Workflow (Dissolve Blend):**
```text
> load l0.bmp
> new 800 600 overlay
> overlay = l0
> select overlay
> filter sharpen

> l0 = l0 * 0.7 + overlay * 0.3
> select l0
> save sharpened_blend.png
```

**The Workflow (Difference Map):**
```text
> load l0.bmp
> new 800 600 after
> after = l0
> select after
> filter blur 10

> new 800 600 diff
> diff = l0 - after
> select diff
> save diff_map.png
```

**How it works:** The dissolve blend mixes 70% of the original with 30% of a sharpened version — a technique that enhances micro-contrast while avoiding the "crunchy" look of full sharpening. The difference map subtracts the filtered layer from the original, leaving only the pixels the filter changed. On a uniform blur, this produces a map of edge information — effectively a manual high-pass extraction, similar to Recipe 2 but computed at the compositing stage rather than within a kernel.

---

## Quick Reference

### Commands

| Command | Syntax | Notes |
|---------|--------|-------|
| `load` | `load <filename>` | Loads BMP; auto-fits to canvas size |
| `new` | `new <w> <h> <name>` | Creates a blank named layer |
| `select` | `select <name>` | Activates a layer by name |
| `color` | `color <r> <g> <b> [a]` | Sets draw color (0–255 per channel) |
| `draw` | `draw <shape> <args>` | See shapes table below |
| `filter` | `filter <name> [radius]` | See filters table below |
| `save` | `save [filename]` | Exports active layer as PNG |
| `clear` | `clear [name\|all]` | Deletes a layer or all layers |
| `monitor` | `monitor` | Displays live web monitor URL |
| `list` | `list` | Lists files in current directory |
| `help` | `help [cmd]` | Shows help for a command |

### Draw Shapes

| Shape | Syntax |
|-------|--------|
| Line | `draw line x0 y0 x1 y1` |
| Rectangle (outline) | `draw rect x0 y0 x1 y1` |
| Rectangle (filled) | `draw fillrect x0 y0 x1 y1` |
| Circle (outline) | `draw circle cx cy radius` |
| Circle (filled) | `draw fillcircle cx cy radius` |
| Triangle (outline) | `draw triangle x0 y0 x1 y1 x2 y2` |
| Triangle (filled) | `draw filltriangle x0 y0 x1 y1 x2 y2` |

### Filters

| Filter | Type | Effect |
|--------|------|--------|
| `blur <radius>` | Separable Gaussian | Smooth blur; large radii possible |
| `gaussian3` | 3×3 Gaussian | Fast, subtle smooth |
| `gaussian5` | 5×5 Gaussian | Higher fidelity smooth |
| `box_blur` | 3×3 Box | Uniform average |
| `edge1` | 3×3 Laplacian (4-way) | Edge detection |
| `edge2` | 3×3 Laplacian (8-way) | Stronger edge detection |
| `sharpen` | 3×3 Unsharp | General sharpening |
| `unsharp5` | 5×5 Unsharp | Detail amplification |
| `emboss` | 3×3 Emboss | Directional depth effect |
| `sobel_x` | 3×3 Sobel | Vertical gradient |
| `sobel_y` | 3×3 Sobel | Horizontal gradient |
| `identity` | 3×3 Passthrough | No-op (testing) |

### Math Evaluator

Expressions are entered as assignment statements: `<dest> = <expr>`

```text
> result = l0 + l1 * 0.5        # 50% blend additive
> result = l0 * 0.7 + l1 * 0.3  # 70/30 dissolve
> result = l0 - l1               # Difference map
> result = l0[r] * 0.0 + l0[g,b,a]  # Zero out red channel
```

Channel selectors (`[r]`, `[g]`, `[b]`, `[a]`, `[r,g,b]`, `[g,b,a]`, etc.) isolate specific channels for read, while the destination always receives a full RGBA result.
