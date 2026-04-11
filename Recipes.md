# Cursed Image Editor: Advanced Compositing Recipes

Welcome to the **Cursed Image Editor** compositing guide. 

Because this engine features a multi-layer architecture, a full abstract syntax tree (AST) math evaluator, channel masking, and 4K-capable separable convolutions, it operates less like a standard paint program and more like a command-line node-graph compositor. 

Below are five advanced use cases that demonstrate how to chain these systems together to create procedural, non-destructive visual effects.

---

### 1. The "Retro 3D" Chromatic Aberration
By isolating the color channels and applying a heavy separable blur *only* to the Red channel, we can simulate the dreamy, out-of-focus "3D glasses" effect heavily used in cyberpunk and synthwave aesthetics.

**The Workflow:**
```text
> load photo.bmp          
> new 800 600 blur_layer  
> l1 = l0                 
> select 1
> filter blur 15          
> l0 = l1[r] + l0[g,b,a]  
> select 0
> save out_retro.png
```
**How it works:** We duplicate the image (`l1`), apply a dynamic 15-pixel Gaussian blur, and then use the math evaluator's channel masking (`l1[r]`) to composite the blurry red data back over the crisp green, blue, and alpha data of the original image (`l0`).

---

### 2. True "High-Pass" Sharpening
Multiplying an edge map over an image creates a dark, gritty "grunge" effect. For a professional, photorealistic sharpen that makes textures pop without ruining the overall brightness of the image, we add a subtle, high-pass edge map back onto the original.

**The Workflow:**
```text
> load photo.bmp          
> new 800 600 edges  
> l1 = l0
> select 1
> filter edge2            
> l0 = l0 + (l1 * 0.3)    
> select 0
> save out_sharp.png
```
**How it works:** The `edge2` (8-connected Laplacian) kernel extracts the high-frequency details. We scale that detail down by 70% (`l1 * 0.3`) to prevent blown-out pixels, and linearly add it back to the original colors.

---

### 3. The "Neon Edge" Colorizer
The Sobel filters detect edges based on harsh gradients. By combining the horizontal and vertical passes, we can extract a perfect outline of the subject. We can then use channel math to kill the red data and boost the blue and green, resulting in a glowing cyan neon outline.

**The Workflow:**
```text
> load subject.bmp        
> new 800 600 sobel_x     
> new 800 600 sobel_y     
> l1 = l0
> l2 = l0
> select 1
> filter sobel_x
> select 2
> filter sobel_y
> l0 = l1 + l2            
> l0 = l0[r] * 0.0 + l0[g] * 1.5 + l0[b] * 2.0 + l0[a] 
> select 0
> save neon.png
```
**How it works:** Since we lack power operators for a true Pythagorean combination, adding the X and Y edge maps (`l1 + l2`) provides a highly accurate approximation. The final line acts as a programmatic color-grading node, zeroing out the red channel and over-saturating the green and blue.

---

### 4. Cinematic Bloom (Screen Blending)
Bloom simulates intense light bleeding over the edges of bright objects, mimicking a physical camera lens. This is achieved by creating a heavily blurred, blown-out copy of the image and adding a fraction of that light back on top.

**The Workflow:**
```text
> load render.bmp         
> new 800 600 bloom       
> l1 = l0
> select 1
> filter blur 40          
> l0 = l0 + (l1 * 0.45)    
> select 0
> save dreamy.png
```
**How it works:** A massive 40-pixel separable blur diffuses the light across the canvas. Adding `45%` of that blurred light back to the original mathematically simulates a "Screen" or "Linear Dodge" blend mode, raising the black levels and making highlights glow softly.

---

### 5. Geometric Emboss Masking
You can combine the built-in rasterization tools with the math evaluator to create dynamic alpha masks. This allows you to restrict heavy convolutions—like an Emboss filter—to specific geometric areas, simulating stamped metal or frosted glass.

**The Workflow:**
```text
> load texture.bmp        
> new 800 600 emboss      
> new 800 600 mask        
> l1 = l0                 
> select 1
> filter emboss           

> select 2
> color 255 255 255 255   
> draw fillcircle 400 300 150 
> filter blur 10          

> l0 = (l1 * l2[r]) + (l0 * (1.0 - l2[r])) 
> select 0
> save stamped_coin.png
```
**How it works:** 1. `l1` holds a fully embossed version of the texture.
2. `l2` holds a white circle, which we blur by 10 pixels to create a soft, anti-aliased edge.
3. The final equation is a true programmatic **Lerp (Linear Interpolation)**. Where the mask (`l2`) is pure white (`1.0`), the equation outputs 100% of the embossed layer. Where the mask is black (`0.0`), the equation outputs 100% of the original texture layer. The blurred edges blend seamlessly between the two!