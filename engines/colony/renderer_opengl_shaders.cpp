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
#include "common/system.h"
#include "common/textconsole.h"

#include "colony/renderer.h"
#include "colony/renderer_opengl_shaders.h"

#ifdef USE_OPENGL_SHADERS

#include "graphics/opengl/system_headers.h"

namespace Colony {

// Phase 1: skeleton only. Every Renderer override is a stub. Callers can
// build with USE_OPENGL_SHADERS, instantiate the renderer (e.g. via
// `--renderer=opengl_shaders`), but the screen will be empty until later
// phases fill in primitives, 3D draws, and the surface blit path.
//
// The fixed-function path (`renderer_opengl.cpp`) remains the default and
// is unaffected by this file.
class OpenGLShaderRenderer : public Renderer {
public:
	OpenGLShaderRenderer(OSystem *system, int width, int height)
		: _system(system), _width(width), _height(height) {
		(void)_width;  // populated for Phase 2+ primitive implementations
		(void)_height;
		warning("Colony: OpenGL shader renderer is a Phase 1 skeleton — "
			"primitives are stubbed; use --renderer=opengl for the working renderer");
	}

	~OpenGLShaderRenderer() override {}

	// 2D primitives — stubbed.
	void clear(uint32 color) override {}
	void drawLine(int x1, int y1, int x2, int y2, uint32 color) override {}
	void drawRect(const Common::Rect &rect, uint32 color) override {}
	void fillRect(const Common::Rect &rect, uint32 color) override {}
	void drawString(const Graphics::Font *font, const Common::String &str, int x, int y,
			uint32 color, Graphics::TextAlign align) override {}
	void scroll(int dx, int dy, uint32 background) override {}
	void drawEllipse(int x, int y, int rx, int ry, uint32 color) override {}
	void fillEllipse(int x, int y, int rx, int ry, uint32 color) override {}
	void fillDitherRect(const Common::Rect &rect, uint32 c1, uint32 c2) override {}
	void setPixel(int x, int y, uint32 color) override {}
	void drawQuad(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4, uint32 color) override {}
	void drawPolygon(const int *x, const int *y, int count, uint32 color) override {}

	void setPalette(const byte *palette, uint start, uint count) override {}

	// 3D scene rendering — stubbed.
	void begin3D(int camX, int camY, int camZ, int angle, int angleY, const Common::Rect &viewport) override {}
	void draw3DWall(int x1, int y1, int x2, int y2, uint32 color) override {}
	void draw3DQuad(float x1, float y1, float z1, float x2, float y2, float z2,
			float x3, float y3, float z3, float x4, float y4, float z4, uint32 color) override {}
	void draw3DPolygon(const float *x, const float *y, const float *z, int count, uint32 color) override {}
	void draw3DLine(float x1, float y1, float z1, float x2, float y2, float z2, uint32 color) override {}
	void end3D() override {}

	// Buffer / state management.
	void copyToScreen() override { _system->updateScreen(); }
	void setWireframe(bool enable, int64_t fillColor) override {}
	void setXorMode(bool enable) override {}
	void setStippleData(const byte *data) override {}
	void setMacColors(uint32 fg, uint32 bg) override {}
	void setDepthState(bool testEnabled, bool writeEnabled) override {}
	void setDepthRange(float nearVal, float farVal) override {}
	void computeScreenViewport() override {}

	void drawSurface(const Graphics::Surface *surf, int x, int y) override {}
	Graphics::Surface *getScreenshot() override { return nullptr; }

private:
	OSystem *_system = nullptr;
	int _width = 0;
	int _height = 0;
};

Renderer *createOpenGLShaderRenderer(OSystem *system, int width, int height) {
	return new OpenGLShaderRenderer(system, width, height);
}

} // End of namespace Colony

#else

namespace Colony {
Renderer *createOpenGLShaderRenderer(OSystem *system, int width, int height) { return nullptr; }
}

#endif
