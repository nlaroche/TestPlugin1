/*
  ==============================================================================
    DelayWave - Plugin Editor Implementation
    A wavey modulated delay effect
  ==============================================================================
*/

#include "PluginEditor.h"
#include "ParameterIDs.h"

#if BEATCONNECT_ACTIVATION_ENABLED
#include <beatconnect/Activation.h>
#endif

static constexpr const char* DEV_SERVER_URL = "http://localhost:5173";

//==============================================================================
DelayWaveEditor::DelayWaveEditor(DelayWaveProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p)
{
    // CRITICAL ORDER - DO NOT CHANGE:
    // 1. setupWebView() - creates relays AND WebBrowserComponent
    // 2. setupRelaysAndAttachments() - connects relays to APVTS
    // 3. setSize() - MUST be AFTER WebView exists so resized() can set bounds

    setupWebView();
    setupRelaysAndAttachments();
    setupActivationEvents();

    setSize(800, 500);
    setResizable(false, false);

    startTimerHz(30);
}

DelayWaveEditor::~DelayWaveEditor()
{
    stopTimer();
}

//==============================================================================
void DelayWaveEditor::setupWebView()
{
    // STEP 1: Create relays BEFORE creating WebBrowserComponent
    timeRelay = std::make_unique<juce::WebSliderRelay>("time");
    feedbackRelay = std::make_unique<juce::WebSliderRelay>("feedback");
    mixRelay = std::make_unique<juce::WebSliderRelay>("mix");
    modRateRelay = std::make_unique<juce::WebSliderRelay>("modRate");
    modDepthRelay = std::make_unique<juce::WebSliderRelay>("modDepth");
    toneRelay = std::make_unique<juce::WebSliderRelay>("tone");
    bypassRelay = std::make_unique<juce::WebToggleButtonRelay>("bypass");

    // STEP 2: Get resources directory
    resourcesDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                       .getParentDirectory()
                       .getChildFile("Resources")
                       .getChildFile("WebUI");

    // STEP 3: Build WebBrowserComponent options
    auto options = juce::WebBrowserComponent::Options()
        .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
        .withNativeIntegrationEnabled()
        .withResourceProvider(
            [this](const juce::String& url) -> std::optional<juce::WebBrowserComponent::Resource>
            {
                auto path = url;
                if (path.startsWith("/"))
                    path = path.substring(1);
                if (path.isEmpty())
                    path = "index.html";

                auto file = resourcesDir.getChildFile(path);
                if (!file.existsAsFile())
                    return std::nullopt;

                juce::String mimeType = "application/octet-stream";
                if (path.endsWith(".html")) mimeType = "text/html";
                else if (path.endsWith(".css")) mimeType = "text/css";
                else if (path.endsWith(".js")) mimeType = "application/javascript";
                else if (path.endsWith(".json")) mimeType = "application/json";
                else if (path.endsWith(".png")) mimeType = "image/png";
                else if (path.endsWith(".jpg") || path.endsWith(".jpeg")) mimeType = "image/jpeg";
                else if (path.endsWith(".svg")) mimeType = "image/svg+xml";
                else if (path.endsWith(".woff")) mimeType = "font/woff";
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
        // Register ALL relays with WebView
        .withOptionsFrom(*timeRelay)
        .withOptionsFrom(*feedbackRelay)
        .withOptionsFrom(*mixRelay)
        .withOptionsFrom(*modRateRelay)
        .withOptionsFrom(*modDepthRelay)
        .withOptionsFrom(*toneRelay)
        .withOptionsFrom(*bypassRelay)
        // Activation event listeners
        .withEventListener("getActivationStatus", [this](const juce::var&) {
            sendActivationState();
        })
        .withEventListener("activate", [this](const juce::var& params) {
            handleActivate(params);
        })
        .withWinWebView2Options(
            juce::WebBrowserComponent::Options::WinWebView2()
                .withBackgroundColour(juce::Colour(0xff0f0f12))
                .withStatusBarDisabled()
                .withUserDataFolder(
                    juce::File::getSpecialLocation(juce::File::tempDirectory)
                        .getChildFile("DelayWave_WebView2")));

    webView = std::make_unique<juce::WebBrowserComponent>(options);
    addAndMakeVisible(*webView);

    // STEP 4: Load URL
#if DELAYWAVE_DEV_MODE
    webView->goToURL(DEV_SERVER_URL);
#else
    webView->goToURL(webView->getResourceProviderRoot());
#endif
}

//==============================================================================
void DelayWaveEditor::setupRelaysAndAttachments()
{
    auto& apvts = processorRef.getAPVTS();

    timeAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::time), *timeRelay, nullptr);
    feedbackAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::feedback), *feedbackRelay, nullptr);
    mixAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::mix), *mixRelay, nullptr);
    modRateAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::modRate), *modRateRelay, nullptr);
    modDepthAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::modDepth), *modDepthRelay, nullptr);
    toneAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *apvts.getParameter(ParamIDs::tone), *toneRelay, nullptr);
    bypassAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment>(
        *apvts.getParameter(ParamIDs::bypass), *bypassRelay, nullptr);
}

//==============================================================================
void DelayWaveEditor::timerCallback()
{
    sendVisualizerData();
    sendActivationState();
}

void DelayWaveEditor::sendVisualizerData()
{
    if (!webView)
        return;

    juce::DynamicObject::Ptr data = new juce::DynamicObject();
    data->setProperty("inputLevel", processorRef.getInputLevel());
    data->setProperty("outputLevel", processorRef.getOutputLevel());
    webView->emitEventIfBrowserIsVisible("visualizerData", juce::var(data.get()));
}

//==============================================================================
void DelayWaveEditor::setupActivationEvents()
{
    // Event listeners are now registered in withEventListener() in setupWebView()
    // This function is kept for any additional runtime setup if needed
}

void DelayWaveEditor::sendActivationState()
{
    if (!webView)
        return;

#if BEATCONNECT_ACTIVATION_ENABLED
    auto* activation = processorRef.getActivation();
    if (activation)
    {
        juce::DynamicObject::Ptr data = new juce::DynamicObject();
        data->setProperty("isConfigured", processorRef.hasActivationEnabled());
        data->setProperty("isActivated", activation->isActivated());

        if (auto info = activation->getActivationInfo())
        {
            data->setProperty("activationCode", juce::String(info->activationCode));
            data->setProperty("expiresAt", juce::String(info->expiresAt));
        }

        webView->emitEventIfBrowserIsVisible("activationState", juce::var(data.get()));
    }
    else
    {
        // Activation SDK not initialized - treat as not configured
        juce::DynamicObject::Ptr data = new juce::DynamicObject();
        data->setProperty("isConfigured", false);
        data->setProperty("isActivated", true);
        webView->emitEventIfBrowserIsVisible("activationState", juce::var(data.get()));
    }
#else
    juce::DynamicObject::Ptr data = new juce::DynamicObject();
    data->setProperty("isConfigured", false);  // No activation configured
    data->setProperty("isActivated", true);
    webView->emitEventIfBrowserIsVisible("activationState", juce::var(data.get()));
#endif
}

void DelayWaveEditor::handleActivate(const juce::var& params)
{
    if (!webView)
        return;

    auto code = params.getProperty("code", "").toString().toStdString();
    if (code.empty())
    {
        sendActivationResult(false, "Invalid", "No activation code provided");
        return;
    }

#if BEATCONNECT_ACTIVATION_ENABLED
    auto* activation = processorRef.getActivation();
    if (!activation)
    {
        sendActivationResult(false, "NotConfigured", "Activation not configured");
        return;
    }

    // Perform activation asynchronously
    activation->activateAsync(code, [this](beatconnect::ActivationStatus status) {
        juce::MessageManager::callAsync([this, status]() {
            bool success = (status == beatconnect::ActivationStatus::Valid ||
                           status == beatconnect::ActivationStatus::AlreadyActive);
            sendActivationResult(success,
                                beatconnect::activationStatusToString(status),
                                success ? "" : "Activation failed");

            if (success)
            {
                sendActivationState();
            }
        });
    });
#else
    // Activation not compiled in - always succeed for testing
    sendActivationResult(true, "Valid", "");
#endif
}

void DelayWaveEditor::sendActivationResult(bool success, const juce::String& status, const juce::String& message)
{
    if (!webView)
        return;

    juce::DynamicObject::Ptr data = new juce::DynamicObject();
    data->setProperty("success", success);
    data->setProperty("status", status);
    if (message.isNotEmpty())
        data->setProperty("message", message);

    webView->emitEventIfBrowserIsVisible("activationResult", juce::var(data.get()));
}

//==============================================================================
void DelayWaveEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0f0f12));
}

void DelayWaveEditor::resized()
{
    if (webView)
        webView->setBounds(getLocalBounds());
}
