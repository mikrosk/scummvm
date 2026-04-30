/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Based on the original sources
 *   https://github.com/Croquetx/thecolony
 * Copyright (C) 1988, David A. Smith
 *
 * Distributed under Apache Version 2.0 License
 *
 */

#include "common/scummsys.h"
#include "common/config-manager.h"
#include "common/debug.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "graphics/font.h"
#include "graphics/surface.h"
#include "math/matrix4.h"

#include "colony/renderer.h"
#include "colony/renderer_opengl_shaders.h"

#ifdef USE_OPENGL_SHADERS

#include "graphics/opengl/shader.h"
#include "graphics/opengl/system_headers.h"

namespace Colony {

// Phase 2: programmable-pipeline 2D primitives. The 3D path (begin3D and
// the corridor draws) and the deprecated state setters (XOR, polygon
// stipple, wireframe) remain stubs and are filled in by later phases.
class OpenGLShaderRenderer : public Renderer {
public:
	OpenGLShaderRenderer(OSystem *system, int width, int height);
	~OpenGLShaderRenderer() override;

	void clear(uint32 color) override;
	void drawLine(int x1, int y1, int x2, int y2, uint32 color) override;
	void drawRect(const Common::Rect &rect, uint32 color) override;
	void fillRect(const Common::Rect &rect, uint32 color) override;
	void drawString(const Graphics::Font *font, const Common::String &str, int x, int y,
			uint32 color, Graphics::TextAlign align) override;
	void scroll(int dx, int dy, uint32 background) override {}
	void drawEllipse(int x, int y, int rx, int ry, uint32 color) override;
	void fillEllipse(int x, int y, int rx, int ry, uint32 color) override;
	void fillDitherRect(const Common::Rect &rect, uint32 c1, uint32 c2) override;
	void setPixel(int x, int y, uint32 color) override;
	void drawQuad(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4, uint32 color) override;
	void drawPolygon(const int *x, const int *y, int count, uint32 color) override;

	void setPalette(const byte *palette, uint start, uint count) override;

	// 3D path — Phase 3.
	void begin3D(int camX, int camY, int camZ, int angle, int angleY, const Common::Rect &viewport) override {}
	void draw3DWall(int x1, int y1, int x2, int y2, uint32 color) override {}
	void draw3DQuad(float x1, float y1, float z1, float x2, float y2, float z2,
			float x3, float y3, float z3, float x4, float y4, float z4, uint32 color) override {}
	void draw3DPolygon(const float *x, const float *y, const float *z, int count, uint32 color) override {}
	void draw3DLine(float x1, float y1, float z1, float x2, float y2, float z2, uint32 color) override {}
	void end3D() override {}

	void copyToScreen() override;
	void setWireframe(bool enable, int64_t fillColor) override {}
	void setXorMode(bool enable) override {}
	void setStippleData(const byte *data) override {}
	void setMacColors(uint32 fg, uint32 bg) override {}
	void setDepthState(bool testEnabled, bool writeEnabled) override {}
	void setDepthRange(float nearVal, float farVal) override {}
	void computeScreenViewport() override;

	void drawSurface(const Graphics::Surface *surf, int x, int y) override;
	Graphics::Surface *getScreenshot() override;

private:
	void resolveColor(uint32 color, float rgba[4]) const;
	void rebuildProjection();
	void uploadSolid(const float *positions, int vertCount);
	void drawSolid(GLenum mode, const float *positions, int vertCount, const float rgba[4]);
	void drawTexturedQuad(int x, int y, int w, int h);

	OSystem *_system = nullptr;
	int _width = 0;
	int _height = 0;
	byte _palette[256 * 3] = {};
	Common::Rect _screenViewport;

	OpenGL::Shader *_solidShader = nullptr;
	OpenGL::Shader *_bitmapShader = nullptr;
	GLuint _solidVBO = 0;
	GLuint _bitmapVBO = 0;
	GLuint _bitmapTexture = 0;

	Math::Matrix4 _projection;

	// Solid VBO holds vec2 position only. Sized for the worst-case 2D
	// primitive — the dither overlay can stream up to width*height/2 dots,
	// so leave room for typical screens (≈170k floats for an 800×600 split).
	enum { kSolidVertexCapacity = 320 * 1024 };
};

// ---------------------------------------------------------------------------
// Construction / teardown
// ---------------------------------------------------------------------------

OpenGLShaderRenderer::OpenGLShaderRenderer(OSystem *system, int width, int height)
	: _system(system), _width(width), _height(height) {
	debug(1, "Colony: using OpenGL shader renderer (Phase 2: 2D primitives "
		"functional; corridor 3D view is stubbed until Phase 3)");
	for (int i = 0; i < 256 * 3; i++)
		_palette[i] = 255;

	static const char *solidAttribs[] = { "position", nullptr };
	_solidShader = OpenGL::Shader::fromFiles("colony_solid", solidAttribs);
	_solidVBO = OpenGL::Shader::createBuffer(GL_ARRAY_BUFFER,
		sizeof(float) * 2 * kSolidVertexCapacity, nullptr, GL_DYNAMIC_DRAW);
	_solidShader->enableVertexAttribute("position", _solidVBO, 2, GL_FLOAT, GL_FALSE,
		2 * sizeof(float), 0);

	static const char *bitmapAttribs[] = { "position", "texcoord", nullptr };
	_bitmapShader = OpenGL::Shader::fromFiles("colony_bitmap", bitmapAttribs);
	// Per-draw vec2 position + vec2 texcoord, 4 vertices for a quad.
	_bitmapVBO = OpenGL::Shader::createBuffer(GL_ARRAY_BUFFER,
		sizeof(float) * 16, nullptr, GL_DYNAMIC_DRAW);
	_bitmapShader->enableVertexAttribute("position", _bitmapVBO, 2, GL_FLOAT, GL_FALSE,
		4 * sizeof(float), 0);
	_bitmapShader->enableVertexAttribute("texcoord", _bitmapVBO, 2, GL_FLOAT, GL_FALSE,
		4 * sizeof(float), 2 * sizeof(float));

	glGenTextures(1, &_bitmapTexture);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);

	computeScreenViewport();
	rebuildProjection();

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
}

OpenGLShaderRenderer::~OpenGLShaderRenderer() {
	OpenGL::Shader::freeBuffer(_solidVBO);
	OpenGL::Shader::freeBuffer(_bitmapVBO);
	delete _solidShader;
	delete _bitmapShader;
	if (_bitmapTexture)
		glDeleteTextures(1, &_bitmapTexture);
}

// ---------------------------------------------------------------------------
// Color resolution and matrix setup
// ---------------------------------------------------------------------------

void OpenGLShaderRenderer::resolveColor(uint32 color, float rgba[4]) const {
	// Same convention as the fixed-function renderer: high byte 0xFF →
	// direct ARGB, otherwise palette index (low byte).
	if (color & 0xFF000000) {
		rgba[0] = ((color >> 16) & 0xFF) / 255.0f;
		rgba[1] = ((color >> 8) & 0xFF) / 255.0f;
		rgba[2] = (color & 0xFF) / 255.0f;
	} else {
		const uint32 idx = color & 0xFF;
		rgba[0] = _palette[idx * 3] / 255.0f;
		rgba[1] = _palette[idx * 3 + 1] / 255.0f;
		rgba[2] = _palette[idx * 3 + 2] / 255.0f;
	}
	rgba[3] = 1.0f;
}

void OpenGLShaderRenderer::rebuildProjection() {
	// Build glOrtho(0, _width, _height, 0, -1, 1) row-major in Math::Matrix4,
	// then transpose so glUniformMatrix4fv (which reads column-major) sees
	// the intended matrix. Y is flipped because our engine puts y=0 at top.
	Math::Matrix4 m;
	for (int r = 0; r < 4; r++)
		for (int c = 0; c < 4; c++)
			m(r, c) = 0.0f;
	m(0, 0) = 2.0f / (float)_width;
	m(0, 3) = -1.0f;
	m(1, 1) = -2.0f / (float)_height;
	m(1, 3) = 1.0f;
	m(2, 2) = -1.0f;
	m(3, 3) = 1.0f;
	m.transpose();
	_projection = m;
}

// ---------------------------------------------------------------------------
// Solid-color primitives
// ---------------------------------------------------------------------------

void OpenGLShaderRenderer::uploadSolid(const float *positions, int vertCount) {
	glBindBuffer(GL_ARRAY_BUFFER, _solidVBO);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * 2 * vertCount, positions);
}

void OpenGLShaderRenderer::drawSolid(GLenum mode, const float *positions, int vertCount,
		const float rgba[4]) {
	if (vertCount <= 0)
		return;
	uploadSolid(positions, vertCount);
	_solidShader->use();
	_solidShader->setUniform("projection", _projection);
	_solidShader->setUniform("color", Math::Vector4d(rgba[0], rgba[1], rgba[2], rgba[3]));
	glDrawArrays(mode, 0, vertCount);
	_solidShader->unbind();
}

void OpenGLShaderRenderer::clear(uint32 color) {
	float rgba[4];
	resolveColor(color, rgba);
	glClearColor(rgba[0], rgba[1], rgba[2], 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void OpenGLShaderRenderer::drawLine(int x1, int y1, int x2, int y2, uint32 color) {
	float rgba[4];
	resolveColor(color, rgba);
	const float verts[] = { (float)x1, (float)y1, (float)x2, (float)y2 };
	drawSolid(GL_LINES, verts, 2, rgba);
}

void OpenGLShaderRenderer::drawRect(const Common::Rect &rect, uint32 color) {
	float rgba[4];
	resolveColor(color, rgba);
	const float verts[] = {
		(float)rect.left,  (float)rect.top,
		(float)rect.right, (float)rect.top,
		(float)rect.right, (float)rect.bottom,
		(float)rect.left,  (float)rect.bottom
	};
	drawSolid(GL_LINE_LOOP, verts, 4, rgba);
}

void OpenGLShaderRenderer::fillRect(const Common::Rect &rect, uint32 color) {
	float rgba[4];
	resolveColor(color, rgba);
	// TRIANGLE_STRIP order: top-left, top-right, bottom-left, bottom-right
	const float verts[] = {
		(float)rect.left,  (float)rect.top,
		(float)rect.right, (float)rect.top,
		(float)rect.left,  (float)rect.bottom,
		(float)rect.right, (float)rect.bottom
	};
	drawSolid(GL_TRIANGLE_STRIP, verts, 4, rgba);
}

void OpenGLShaderRenderer::setPixel(int x, int y, uint32 color) {
	// Same as a 1×1 fillRect — this is rarely called now that animation/PICT
	// blits go through drawSurface.
	fillRect(Common::Rect(x, y, x + 1, y + 1), color);
}

void OpenGLShaderRenderer::drawQuad(int x1, int y1, int x2, int y2,
		int x3, int y3, int x4, int y4, uint32 color) {
	float rgba[4];
	resolveColor(color, rgba);
	// Filled body (TRIANGLE_FAN handles convex quads correctly).
	const float fanVerts[] = {
		(float)x1, (float)y1,
		(float)x2, (float)y2,
		(float)x3, (float)y3,
		(float)x4, (float)y4
	};
	drawSolid(GL_TRIANGLE_FAN, fanVerts, 4, rgba);

	// Match the fixed-function renderer's white outline overlay.
	const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	drawSolid(GL_LINE_LOOP, fanVerts, 4, white);
}

void OpenGLShaderRenderer::drawPolygon(const int *x, const int *y, int count, uint32 color) {
	if (count < 3)
		return;
	if (count > 1024)
		count = 1024;

	float rgba[4];
	resolveColor(color, rgba);

	float verts[2 * 1024];
	for (int i = 0; i < count; i++) {
		verts[i * 2 + 0] = (float)x[i];
		verts[i * 2 + 1] = (float)y[i];
	}
	drawSolid(GL_TRIANGLE_FAN, verts, count, rgba);

	// Same white outline as drawQuad.
	const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	drawSolid(GL_LINE_LOOP, verts, count, white);
}

void OpenGLShaderRenderer::drawEllipse(int x, int y, int rx, int ry, uint32 color) {
	float rgba[4];
	resolveColor(color, rgba);
	const int kSegments = 36; // 10° steps, matching the fixed-function path
	float verts[kSegments * 2];
	for (int i = 0; i < kSegments; i++) {
		const float rad = i * 2.0f * (float)M_PI / kSegments;
		verts[i * 2 + 0] = x + cosf(rad) * (float)rx;
		verts[i * 2 + 1] = y + sinf(rad) * (float)ry;
	}
	drawSolid(GL_LINE_LOOP, verts, kSegments, rgba);
}

void OpenGLShaderRenderer::fillEllipse(int x, int y, int rx, int ry, uint32 color) {
	float rgba[4];
	resolveColor(color, rgba);
	const int kSegments = 36;
	float verts[kSegments * 2];
	for (int i = 0; i < kSegments; i++) {
		const float rad = i * 2.0f * (float)M_PI / kSegments;
		verts[i * 2 + 0] = x + cosf(rad) * (float)rx;
		verts[i * 2 + 1] = y + sinf(rad) * (float)ry;
	}
	drawSolid(GL_TRIANGLE_FAN, verts, kSegments, rgba);
}

void OpenGLShaderRenderer::fillDitherRect(const Common::Rect &rect, uint32 c1, uint32 c2) {
	fillRect(rect, c1);
	float rgba[4];
	resolveColor(c2, rgba);

	const int w = rect.width();
	const int h = rect.height();
	if (w <= 0 || h <= 0)
		return;

	// 50% checkerboard: place a dot on every other pixel, alternating per row.
	// Capacity guard — fall back to solid c2 if the rect is larger than our
	// streaming buffer (very rare: only the dashboard background hits this
	// path, and it is much smaller than kSolidVertexCapacity).
	const int maxDots = kSolidVertexCapacity;
	int dots = 0;
	float *verts = new float[maxDots * 2];
	for (int yi = 0; yi < h && dots < maxDots; yi++) {
		const int yy = rect.top + yi;
		for (int xi = (yi & 1); xi < w && dots < maxDots; xi += 2) {
			verts[dots * 2 + 0] = (float)(rect.left + xi);
			verts[dots * 2 + 1] = (float)yy;
			dots++;
		}
	}
	if (dots > 0)
		drawSolid(GL_POINTS, verts, dots, rgba);
	delete[] verts;
}

// ---------------------------------------------------------------------------
// Text rendering
// ---------------------------------------------------------------------------

void OpenGLShaderRenderer::drawString(const Graphics::Font *font, const Common::String &str,
		int x, int y, uint32 color, Graphics::TextAlign align) {
	if (!font)
		return;
	const int w = font->getStringWidth(str);
	const int h = font->getFontHeight();
	if (w <= 0 || h <= 0)
		return;

	if (align == Graphics::kTextAlignCenter)
		x -= w / 2;
	else if (align == Graphics::kTextAlignRight)
		x -= w;

	float rgba[4];
	resolveColor(color, rgba);

	// Render glyphs to a 1-byte-per-pixel mask, then build an RGBA image
	// where set pixels carry the requested color (alpha 1) and unset
	// pixels are transparent. Upload as a texture and draw a single quad.
	Graphics::Surface mask;
	mask.create(w, h, Graphics::PixelFormat::createFormatCLUT8());
	memset(mask.getPixels(), 0, w * h);
	font->drawString(&mask, str, 0, 0, w, 1, Graphics::kTextAlignLeft);

	const byte cr = (byte)(rgba[0] * 255.0f);
	const byte cg = (byte)(rgba[1] * 255.0f);
	const byte cb = (byte)(rgba[2] * 255.0f);

	uint32 *rgbaBuf = new uint32[w * h];
	for (int py = 0; py < h; py++) {
		const byte *src = (const byte *)mask.getBasePtr(0, py);
		uint32 *dst = rgbaBuf + py * w;
		for (int px = 0; px < w; px++) {
			if (src[px] == 1)
				dst[px] = ((uint32)cr) | ((uint32)cg << 8) | ((uint32)cb << 16) | (0xFFu << 24);
			else
				dst[px] = 0;
		}
	}
	mask.free();

	glBindTexture(GL_TEXTURE_2D, _bitmapTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgbaBuf);
	delete[] rgbaBuf;

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	drawTexturedQuad(x, y, w, h);
	glDisable(GL_BLEND);
}

// ---------------------------------------------------------------------------
// Surface blit
// ---------------------------------------------------------------------------

void OpenGLShaderRenderer::drawSurface(const Graphics::Surface *surf, int x, int y) {
	if (!surf || surf->w <= 0 || surf->h <= 0)
		return;
	if (surf->format.bytesPerPixel != 4)
		return;

	glBindTexture(GL_TEXTURE_2D, _bitmapTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	// The engine's surface format is PixelFormat(4,8,8,8,8,24,16,8,0) —
	// R at bit 24, A at bit 0 — so GL_RGBA + GL_UNSIGNED_INT_8_8_8_8 reads
	// each uint32 with the high byte mapping to R, matching the fixed-
	// function path's drawSurface upload (renderer_opengl.cpp:725).
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surf->w, surf->h, 0,
		GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, surf->getPixels());

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	drawTexturedQuad(x, y, surf->w, surf->h);
	glDisable(GL_BLEND);
}

void OpenGLShaderRenderer::drawTexturedQuad(int x, int y, int w, int h) {
	const float verts[] = {
		// position             texcoord
		(float)x,       (float)y,       0.0f, 0.0f,
		(float)(x + w), (float)y,       1.0f, 0.0f,
		(float)x,       (float)(y + h), 0.0f, 1.0f,
		(float)(x + w), (float)(y + h), 1.0f, 1.0f
	};
	glBindBuffer(GL_ARRAY_BUFFER, _bitmapVBO);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

	_bitmapShader->use();
	_bitmapShader->setUniform("projection", _projection);
	_bitmapShader->setUniform("tex", 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, _bitmapTexture);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	_bitmapShader->unbind();
}

// ---------------------------------------------------------------------------
// Buffer / state management
// ---------------------------------------------------------------------------

void OpenGLShaderRenderer::setPalette(const byte *palette, uint start, uint count) {
	if (start + count > 256)
		count = 256 - start;
	memcpy(_palette + start * 3, palette, count * 3);
}

void OpenGLShaderRenderer::computeScreenViewport() {
	const int32 screenWidth = _system->getWidth();
	const int32 screenHeight = _system->getHeight();
	const bool widescreen = ConfMan.getBool("widescreen_mod");

	if (widescreen) {
		_screenViewport = Common::Rect(screenWidth, screenHeight);
	} else if (_system->getFeatureState(OSystem::kFeatureAspectRatioCorrection)) {
		const int32 vpW = MIN<int32>(screenWidth, screenHeight * 4 / 3);
		const int32 vpH = MIN<int32>(screenHeight, screenWidth * 3 / 4);
		_screenViewport = Common::Rect(vpW, vpH);
		_screenViewport.translate((screenWidth - vpW) / 2, (screenHeight - vpH) / 2);
	} else {
		_screenViewport = Common::Rect(screenWidth, screenHeight);
	}

	glViewport(_screenViewport.left, screenHeight - _screenViewport.bottom,
		_screenViewport.width(), _screenViewport.height());
	glScissor(_screenViewport.left, screenHeight - _screenViewport.bottom,
		_screenViewport.width(), _screenViewport.height());
}

void OpenGLShaderRenderer::copyToScreen() {
	glFlush();
	_system->updateScreen();
}

Graphics::Surface *OpenGLShaderRenderer::getScreenshot() {
	Graphics::Surface *surface = new Graphics::Surface();
	surface->create(_screenViewport.width(), _screenViewport.height(),
		Graphics::PixelFormat::createFormatRGBA32());
	glPixelStorei(GL_PACK_ALIGNMENT, 4);
	glReadPixels(_screenViewport.left, _system->getHeight() - _screenViewport.bottom,
		_screenViewport.width(), _screenViewport.height(),
		GL_RGBA, GL_UNSIGNED_BYTE, surface->getPixels());
	surface->flipVertical(Common::Rect(surface->w, surface->h));
	return surface;
}

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

Renderer *createOpenGLShaderRenderer(OSystem *system, int width, int height) {
	return new OpenGLShaderRenderer(system, width, height);
}

} // End of namespace Colony

#else

namespace Colony {
Renderer *createOpenGLShaderRenderer(OSystem *system, int width, int height) { return nullptr; }
}

#endif
