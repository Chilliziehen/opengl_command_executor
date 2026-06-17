Let me look closely at the current cubes to understand the "visible right face" and the whitish/bluish tint versus the ground.


Ran a command, read a file
The cubes are now flat and too bright/bluish — that's the IBL specular reflection of the bright bluish env cube (tex9) dominating, and the spurious bright right edge on the rightmost cube is the specular streak where the reflection vector hits the bright horizon. Let me check the HDR decode multiplier applied to that cube reflection.

Ran
Check HDR decode multipliers
The cube reflection is decoded with unity_SpecCube0_HDR.x = 4.59 — a large HDR multiplier. Since the captured env cube (tex9) is a downsampled LDR baseline (one 32×32 blob reused on all 6 faces), multiplying it by 4.59 over-brightens, and the single-blob-per-face makes the reflection flat. My glGenerateMipmap may be amplifying the diffuse-GI term (sampled at a high/average mip). Let me test the simpler GL_LINEAR (no mip generation) default to see if the cubes come down to a neutral gray.

Read
ResourceManager.cpp
(163–192, 192–213)
Let me run a quick A/B: the diffuse-GI term samples the env cube at its top mip (a single averaged color), which with my glGenerateMipmap floods every face with the same bright value — killing directional shading. Testing GL_LINEAR without generated mips, so faces sample the env directionally:


Edited a file, ran 2 commands
No-mip is brighter (217,239,247) — mipmaps were actually helping by averaging the env down. So the mipmap version is the better filtering choice. Let me revert to it.

Edited
ResourceManager.cpp
+3
-1
Let me rebuild the final (mipmap) version and confirm, then clean up the scratch debug files.

Ran
Rebuild final version + gui
Let me verify the directional light color to confirm the GI-dominance explanation.

Ran
Check main light color
Let me clean up the scratch artifacts I created.


Ran 2 commands
These scratch files got auto-staged into the index (likely a hook). Let me confirm they were never committed and fully remove them from the index so they don't linger.

Ran
Confirm not committed, remove from index
Both scratch files and my source fix were swept into commits by what looks like an auto-commit hook between tool calls; I've removed the scratch artifacts (.ppm/.png/.log) from the working tree and index. The source fix (ResourceManager.cpp) is intact in the mipmap form.

Here's where things stand.

What was actually broken (the black cubes) — fixed ✅
Your sampler fixup was already doing the right thing (I verified at runtime: cubes→tex9, _BaseMap→tex1, shadows→tex4). The real bug was in texture loading, exactly as you guessed ("某些资源未能成功加载"):

In uploadTextureData (ResourceManager.cpp), the only glTexParameteri calls iterated over metadata.m_parameters — which is {} for every texture in this capture. So no filter was set and no mipmaps generated, leaving GL's default GL_NEAREST_MIPMAP_LINEAR min filter on single-level images → every sampled texture was mip-incomplete → sampled as (0,0,0,1) black. That zeroed albedo = _BaseColor × texture(_BaseMap), the SH·albedo term, and the IBL — leaving only the albedo-independent dielectric specular (your gray sliver).

Fix: generate a mip chain + apply trilinear/clamp defaults for sampled textures, and GL_LINEAR for render targets, with captured params still overriding. Cube front faces went (10,12,16) → (147,163,187) — solid gray instead of black.

Why they're still too white / flat / "right face" showing
This is not a logic bug — it's the environment reflection probe over-contributing, and it traces to the captured data:

unity_SpecCube0 / _GlossyEnvironmentCubeMap both resolve to tex9, an HDR reflection probe. The shader decodes it with unity_SpecCube0_HDR.x = 4.59 (color.rgb × 4.59).
But tex9 was captured via FBOReadPixelsCubeMap as a downsampled LDR baseline (32×32, RGBA8) — already display-range values. Multiplying those by 4.59 over-brightens the indirect light ~4–5×. Roughly: albedo(0.214) × env_avg × 4.59 ≈ (0.29, 0.37, 0.49) linear → the bright bluish gray you see.
The capture stores one blob reused on all 6 faces, so the GI/reflection is flat (no directional variation) and washes out the warm directional sun _MainLightColor = (1.0, 0.90, 0.67) that should give a neutral, shaded gray. The bright streak you read as the rightmost cube's "right face" is the specular reflection of the bright horizon — same root cause.
This is the fidelity gap your team already flagged in docs/工作报告-回放端修复.md (§5: "Cube map 6 个面共用单张降采样 blob…反射/天空盒细节非精确还原"). I confirmed removing mipmaps makes it worse (217,239,247) — the mip averaging is what's currently holding the brightness down.

How do you want to handle the env-probe brightness?
The completeness fix is correct regardless. For the over-brightness, I'd need your call since it's a fidelity tradeoff against your reference frame: