#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParameterIDs.h"

ExamplePluginEditor::ExamplePluginEditor(ExamplePluginProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    // CRITICAL ORDER:
    // 1. setupWebView() - creates relays AND WebView
    // 2. setupAttachments() - connects relays to APVTS
    // 3. setSize() - AFTER WebView exists so resized() can set bounds

    setupWebView();
    setupAttachments();

    setSize(800, 500);
    setResizable(false, false);

    startTimerHz(30);
}

ExamplePluginEditor::~ExamplePluginEditor()
{
    stopTimer();

    // Clean up attachments BEFORE webView
    gainAttachment.reset();
    mixAttachment.reset();
    bypassAttachment.reset();

    webView.reset();
}

void ExamplePluginEditor::setupWebView()
{
    // ===========================================================================
    // STEP 1: Create relays BEFORE WebBrowserComponent
    // ===========================================================================
    gainRelay = std::make_unique<juce::WebSliderRelay>(ParamIDs::gain);
    mixRelay = std::make_unique<juce::WebSliderRelay>(ParamIDs::mix);
    bypassRelay = std::make_unique<juce::WebToggleButtonRelay>(ParamIDs::bypass);

    // ===========================================================================
    // STEP 2: Find resources directory (handle multiple locations)
    // ===========================================================================
    auto executableFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    auto executableDir = executableFile.getParentDirectory();

    resourcesDir = executableDir.getChildFile("Resources").getChildFile("WebUI");
    if (!resourcesDir.isDirectory())
        resourcesDir = executableDir.getChildFile("WebUI");
    if (!resourcesDir.isDirectory())
        resourcesDir = executableDir.getParentDirectory().getChildFile("Resources").getChildFile("WebUI");

    DBG("Resources dir: " + resourcesDir.getFullPathName());

    // ===========================================================================
    // STEP 3: Build WebBrowserComponent options
    // ===========================================================================
    auto options = juce::WebBrowserComponent::Options()
        .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
        .withNativeIntegrationEnabled()
        .withResourceProvider(
            [this](const juce::String& url) -> std::optional<juce::WebBrowserComponent::Resource>
            {
                auto path = url;
                if (path.startsWith("/")) path = path.substring(1);
                if (path.isEmpty()) path = "index.html";

                auto file = resourcesDir.getChildFile(path);
                if (!file.existsAsFile()) return std::nullopt;

                juce::String mimeType = "application/octet-stream";
                if (path.endsWith(".html")) mimeType = "text/html";
                else if (path.endsWith(".css")) mimeType = "text/css";
                else if (path.endsWith(".js")) mimeType = "application/javascript";
                else if (path.endsWith(".json")) mimeType = "application/json";
                else if (path.endsWith(".png")) mimeType = "image/png";
                else if (path.endsWith(".svg")) mimeType = "image/svg+xml";
                else if (path.endsWith(".woff2")) mimeType = "font/woff2";

                juce::MemoryBlock data;
                file.loadFileAsData(data);

                return juce::WebBrowserComponent::Resource{
                    std::vector<std::byte>(
                        reinterpret_cast<const std::byte*>(data.getData()),
                        reinterpret_cast<const std::byte*>(data.getData()) + data.getSize()),
                    mimeType.toStdString()
                };
            })
        // Register relays
        .withOptionsFrom(*gainRelay)
        .withOptionsFrom(*mixRelay)
        .withOptionsFrom(*bypassRelay)
        // Activation status event (always register, even without activation)
        .withEventListener("getActivationStatus", [this](const juce::var&) {
            juce::DynamicObject::Ptr data = new juce::DynamicObject();
#if BEATCONNECT_ACTIVATION_ENABLED
            data->setProperty("isConfigured", true);
            data->setProperty("isActivated", false);  // Would check actual status
#else
            data->setProperty("isConfigured", false);
            data->setProperty("isActivated", false);
#endif
            webView->emitEventIfBrowserIsVisible("activationState", juce::var(data.get()));
        })
        // Windows-specific options
        .withWinWebView2Options(
            juce::WebBrowserComponent::Options::WinWebView2()
                .withBackgroundColour(juce::Colour(0xFF1a1a1a))
                .withStatusBarDisabled()
                .withUserDataFolder(
                    juce::File::getSpecialLocation(juce::File::tempDirectory)
                        .getChildFile("ExamplePlugin_WebView2")));

    // ===========================================================================
    // STEP 4: Create WebBrowserComponent and load URL
    // ===========================================================================
    webView = std::make_unique<juce::WebBrowserComponent>(options);
    addAndMakeVisible(*webView);

#if EXAMPLE_PLUGIN_DEV_MODE
    webView->goToURL("http://localhost:5173");
    DBG("Loading dev server at localhost:5173");
#else
    webView->goToURL(webView->getResourceProviderRoot());
    DBG("Loading from resource provider");
#endif
}

void ExamplePluginEditor::setupAttachments()
{
    auto& apvts = processorRef.getAPVTS();

    gainAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::gain), *gainRelay, nullptr);
    mixAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::mix), *mixRelay, nullptr);
    bypassAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment>(
        *apvts.getParameter(ParamIDs::bypass), *bypassRelay, nullptr);
}

void ExamplePluginEditor::timerCallback()
{
    sendVisualizerData();
}

void ExamplePluginEditor::sendVisualizerData()
{
    if (!webView) return;

    juce::DynamicObject::Ptr data = new juce::DynamicObject();
    data->setProperty("inputLevel", processorRef.getInputLevel());
    data->setProperty("outputLevel", processorRef.getOutputLevel());

    webView->emitEventIfBrowserIsVisible("visualizerData", juce::var(data.get()));
}

void ExamplePluginEditor::sendActivationState()
{
    if (!webView) return;

    juce::DynamicObject::Ptr data = new juce::DynamicObject();
#if BEATCONNECT_ACTIVATION_ENABLED
    data->setProperty("isConfigured", true);
    data->setProperty("isActivated", false);
#else
    data->setProperty("isConfigured", false);
    data->setProperty("isActivated", false);
#endif
    webView->emitEventIfBrowserIsVisible("activationState", juce::var(data.get()));
}

void ExamplePluginEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF1a1a1a));
}

void ExamplePluginEditor::resized()
{
    if (webView)
        webView->setBounds(getLocalBounds());
}
