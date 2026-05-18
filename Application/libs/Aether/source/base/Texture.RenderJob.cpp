#include "Aether/base/Texture.hpp"
#include "Aether/base/Texture.RenderJob.hpp"

namespace Aether {
    Texture::RenderJob::RenderJob(Texture * texture) : Job() {
        this->texture = texture;
    }

    void Texture::RenderJob::work() {
        // Render outside the lock — this is the expensive CPU work.
        Drawable * result = this->texture->renderDrawable();

        // Acquire the per-texture lock before touching tmpDrawable/status.
        // If destroy() already set cancelled=true we must discard our result
        // instead of writing it — the Texture may be about to be deleted.
        std::unique_lock<std::mutex> lk(this->texture->renderMutex);
        if (this->texture->cancelled) {
            // destroy() will clean up; just throw away what we rendered.
            delete result;
            return;
        }
        this->texture->tmpDrawable = result;
        this->texture->status = AsyncStatus::NeedsConvert;
    }
}