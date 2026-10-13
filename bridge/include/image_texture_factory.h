#pragma once
// A texture factory that holds a decoded image and makes the texture on the render thread (no scene graph context is
// needed to create it, so the image providers also work in tests without a window).

#include <QImage>
#include <QQuickTextureFactory>
#include <QQuickWindow>
#include <QSGTexture>

class ImageTextureFactory : public QQuickTextureFactory {
public:
    explicit ImageTextureFactory(QImage image) : image_(std::move(image)) {}
    QSGTexture* createTexture(QQuickWindow* window) const override { return window->createTextureFromImage(image_); }
    QSize textureSize() const override { return image_.size(); }
    int textureByteCount() const override { return static_cast<int>(image_.sizeInBytes()); }
    QImage image() const override { return image_; }

private:
    QImage image_;
};
