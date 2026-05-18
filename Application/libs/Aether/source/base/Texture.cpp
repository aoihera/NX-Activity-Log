#include "Aether/base/Texture.hpp"
#include "Aether/base/Texture.RenderJob.hpp"
#include "Aether/ThreadPool.hpp"

namespace Aether {
    Texture::Texture(const int x, const int y) : Element(x, y, 0, 0) {
        this->asyncID = 0;
        this->onRenderDoneFunc = nullptr;
        this->status = AsyncStatus::Waiting;
        this->cancelled = false;

        this->colour_ = Colour(255, 255, 255, 255);
        this->drawable = new Drawable();
        this->tmpDrawable = nullptr;
    }

    void Texture::setupDrawable() {
        if (this->drawable == nullptr) {
            return;
        }
        this->drawable->convertToTexture();
        this->drawable->setColour(this->colour_);

        this->setW(this->drawable->width());
        this->setH(this->drawable->height());

        if (this->onRenderDoneFunc != nullptr) {
            this->onRenderDoneFunc();
        }
    }

    void Texture::onRenderDone(const std::function<void()> func) {
        this->onRenderDoneFunc = func;
    }

    Colour Texture::colour() {
        return this->colour_;
    }

    void Texture::setColour(const Colour & col) {
        this->colour_ = col;
        if (this->drawable != nullptr) {
            this->drawable->setColour(this->colour_);
        }
    }

    int Texture::textureWidth() {
        return (this->drawable != nullptr) ? this->drawable->width() : 0;
    }

    int Texture::textureHeight() {
        return (this->drawable != nullptr) ? this->drawable->height() : 0;
    }

    void Texture::setMask(const int x, const int y, const unsigned int w, const unsigned int h) {
        if (this->drawable != nullptr) {
            this->drawable->setMask(x, y, w, h);
        }
    }

    void Texture::destroy() {
        if (this->status == AsyncStatus::Rendering) {
            // Signal the worker that it should discard its result, then wait
            // for it to finish.  The order matters:
            //  1. Set cancelled=true under renderMutex so RenderJob::work()
            //     sees it before (or after) writing tmpDrawable.
            //  2. Call removeOrWaitForJob — which now releases jobsMutex before
            //     blocking — so the worker can complete without deadlocking.
            //  3. After the wait returns we hold renderMutex and know the worker
            //     is done; clean up tmpDrawable if the worker left one.
            {
                std::unique_lock<std::mutex> lk(this->renderMutex);
                this->cancelled = true;
            }
            ThreadPool::getInstance()->removeOrWaitForJob(this->asyncID);
            this->asyncID = 0;
        }

        // Under the lock: discard any drawable the worker may have stored.
        {
            std::unique_lock<std::mutex> lk(this->renderMutex);
            delete this->tmpDrawable;
            this->tmpDrawable = nullptr;
            this->cancelled = false;
        }

        delete this->drawable;
        this->drawable = nullptr;
        this->drawable = new Drawable();
        this->status = AsyncStatus::Waiting;
    }

    bool Texture::ready() {
        return (this->drawable->type() == Drawable::Type::Texture);
    }

    void Texture::renderSync() {
        if (this->status != AsyncStatus::Waiting) {
            return;
        }

        Drawable * result = this->renderDrawable();
        delete this->drawable;
        // renderDrawable() should never return nullptr (Renderer always returns
        // at least an empty Drawable on failure), but guard anyway to be safe.
        this->drawable = (result != nullptr) ? result : new Drawable();
        this->setupDrawable();
        this->status = AsyncStatus::Done;
    }

    void Texture::renderAsync() {
        if (this->status != AsyncStatus::Waiting) {
            return;
        }

        this->status = AsyncStatus::Rendering;
        this->asyncID = ThreadPool::getInstance()->queueJob(new RenderJob(this), ThreadPool::Importance::Normal);
    }

    void Texture::update(unsigned int dt) {
        if (this->status == AsyncStatus::NeedsConvert) {
            // Take the lock so we don't race with destroy() cancelling a job
            // that just finished.
            std::unique_lock<std::mutex> lk(this->renderMutex);
            if (!this->cancelled && this->tmpDrawable != nullptr) {
                delete this->drawable;
                this->drawable = this->tmpDrawable;
                this->tmpDrawable = nullptr;
                lk.unlock();

                this->setupDrawable();
                this->status = AsyncStatus::Done;
            } else {
                // Job was cancelled; tmpDrawable already cleaned by destroy().
                this->status = AsyncStatus::Waiting;
            }
        }
        Element::update(dt);
    }

    void Texture::render() {
        if (this->hidden()) {
            return;
        }

        if (this->drawable != nullptr) {
            this->drawable->render(this->x(), this->y(), this->w(), this->h());
        }
        Element::render();
    }

    Texture::~Texture() {
        if (this->status == AsyncStatus::Rendering) {
            {
                std::unique_lock<std::mutex> lk(this->renderMutex);
                this->cancelled = true;
            }
            ThreadPool::getInstance()->removeOrWaitForJob(this->asyncID);
        }
        delete this->drawable;
        // Under the lock in case the worker finished between the wait and here.
        std::unique_lock<std::mutex> lk(this->renderMutex);
        delete this->tmpDrawable;
    }
};