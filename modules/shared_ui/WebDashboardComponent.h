#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include <functional>
#include <optional>

#ifndef JUCE_ASSERT_MESSAGE_THREAD
#define JUCE_ASSERT_MESSAGE_THREAD jassert(juce::MessageManager::getInstance()->isThisTheMessageThread())
#endif

namespace gitpro::ui
{
    class WebDashboardComponent final : public juce::Component
    {
    public:
        using CommandHandler = std::function<void(const juce::String& commandJson)>;

        WebDashboardComponent();

        void resized() override;

        void publishJsonSnapshot(const juce::String& snapshotJson);
        void setCommandHandler(CommandHandler handler);

    private:
        static juce::File getWebViewUserDataFolder();
        static std::optional<juce::WebBrowserComponent::Resource> createDashboardResource(const juce::String& path);
        static juce::String createInitialHtml();

        juce::WebBrowserComponent browser;
        juce::String latestSnapshotJson;
        CommandHandler commandHandler;
    };
}