/**
 * @file ReplayEngine.h
 * @brief Per-command ("逐 cmd") replay engine on top of drawCommandsGLlib.
 *
 * Wraps load → init-state → step/seek so a UI (or test) can scrub through a
 * captured frame one command at a time, RenderDoc-style. Stepping backward or
 * jumping to an earlier command re-initializes GL state and replays from the
 * start (GL has no undo), while stepping forward just executes the next command.
 *
 * Requires a current GL context.
 */
#ifndef REPLAY_ENGINE_H
#define REPLAY_ENGINE_H

#include <cstdint>
#include <string>

#include "framecapture/FrameCapture.h"

class ReplayEngine {
public:
    /// Load a capture directory (CPU only; safe before/without a GL context).
    bool load(const std::string& captureDirectory, std::string& outError);

    /// Release all resources and return to the "no capture loaded" state.
    /// Safe to call multiple times.
    void unload();

    /// (Re)initialize GL to the captured frame-start state and move the cursor
    /// to 0 (no commands executed yet). Requires a current GL context.
    bool reset(std::string& outError);

    /// Execute the next command. Returns false if already at the end.
    bool stepForward();

    /// Step back one command (re-init + replay to cursor-1).
    bool stepBackward(std::string& outError);

    /// Make exactly the first `index` commands executed (index in [0, count]).
    /// Replays forward from the current cursor, or re-initializes first if the
    /// target is behind the cursor.
    bool seekTo(size_t index, std::string& outError);

    // ---- queries ----
    size_t cursor() const { return m_cursor; }              ///< # commands executed
    size_t commandCount() const { return m_capture.commandCount(); }
    bool   isLoaded() const { return m_loaded; }
    bool   isInitialized() const { return m_initialized; }

    const FrameCapture& capture() const { return m_capture; }
    const std::string&  captureDirectory() const { return m_captureDirectory; }

    /// Name of the command at `index` (e.g. "drawElements"), or "" if invalid.
    const std::string& commandName(size_t index) const;
    /// eventId of the command at `index`, or 0 if invalid.
    uint32_t commandEventId(size_t index) const;

    /// GL handle of the framebuffer currently bound for drawing (the render
    /// target the most recent command wrote to). 0 = default framebuffer.
    unsigned int currentDrawFramebuffer() const;

    /// GL error string accumulated during the last step/seek, or "" if none.
    const std::string& lastGlErrors() const { return m_lastGlErrors; }

private:
    FrameCapture m_capture;
    std::string  m_captureDirectory;
    size_t       m_cursor = 0;
    bool         m_loaded = false;
    bool         m_initialized = false;
    std::string  m_lastGlErrors;

    void executeOne(size_t index);
};

#endif // REPLAY_ENGINE_H
