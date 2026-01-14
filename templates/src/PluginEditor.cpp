/*
  ==============================================================================
    {{PLUGIN_NAME}} - Plugin Editor Implementation
    BeatConnect Plugin Template
  ==============================================================================
*/

#include "PluginEditor.h"

//==============================================================================
{{PLUGIN_NAME}}Editor::{{PLUGIN_NAME}}Editor({{PLUGIN_NAME}}Processor& p)
    : AudioProcessorEditor(&p),
      processorRef(p)
{
    // Set initial size
    setSize(800, 600);
    setResizable(true, true);
    setResizeLimits(400, 300, 1920, 1080);

    // Collect parameter IDs for listening
    for (auto* param : processorRef.getAPVTS().processor.getParameters())
    {
        if (auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*>(param))
        {
            parameterIds.add(rangedParam->getParameterID());
            processorRef.getAPVTS().addParameterListener(rangedParam->getParameterID(), this);
        }
    }

    // Create WebView
    juce::WebBrowserComponent::Options options;
    options = options.withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options(
                         juce::WebBrowserComponent::Options::WinWebView2{}
                             .withUserDataFolder(juce::File::getSpecialLocation(
                                 juce::File::tempDirectory).getChildFile("{{PLUGIN_NAME}}_WebView")));

    webView = std::make_unique<juce::WebBrowserComponent>(options);
    addAndMakeVisible(*webView);

    // Set up message handler from web
    webView->setJavascriptChannelCallback("juce", [this](const juce::var& message) {
        handleWebMessage(message);
    });

    // Load web UI
    // In development, you might use a local server:
    // webView->goToURL("http://localhost:5173");

    // In production, load from bundled files:
    auto webDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                      .getParentDirectory()
                      .getChildFile("web");

    if (webDir.exists())
    {
        webView->goToURL(juce::URL(webDir.getChildFile("index.html")));
    }
    else
    {
        // Fallback message
        webView->goToURL("data:text/html,<h1>Web UI not found</h1><p>Build the web UI first: cd web && npm run build</p>");
    }

    // Send initial parameter state after a short delay (let web UI initialize)
    juce::Timer::callAfterDelay(500, [this]() {
        sendParameterState();
    });
}

{{PLUGIN_NAME}}Editor::~{{PLUGIN_NAME}}Editor()
{
    // Remove parameter listeners
    for (const auto& paramId : parameterIds)
    {
        processorRef.getAPVTS().removeParameterListener(paramId, this);
    }
}

//==============================================================================
void {{PLUGIN_NAME}}Editor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
}

void {{PLUGIN_NAME}}Editor::resized()
{
    if (webView)
        webView->setBounds(getLocalBounds());
}

//==============================================================================
void {{PLUGIN_NAME}}Editor::handleWebMessage(const juce::var& message)
{
    // Messages come as JSON strings from web
    juce::String messageStr = message.toString();

    auto json = juce::JSON::parse(messageStr);
    if (json.isVoid())
        return;

    auto type = json.getProperty("type", "").toString();
    auto data = json.getProperty("data", juce::var());

    if (type == "setParameter")
    {
        // Web is setting a parameter value
        auto paramId = data.getProperty("id", "").toString();
        auto value = static_cast<float>(data.getProperty("value", 0.0));

        if (auto* param = processorRef.getAPVTS().getParameter(paramId))
        {
            param->setValueNotifyingHost(param->convertTo0to1(value));
        }
    }
    else if (type == "getParameterState")
    {
        // Web is requesting current parameter state
        sendParameterState();
    }
    else if (type == "ready")
    {
        // Web UI has finished loading
        sendParameterState();
    }
}

void {{PLUGIN_NAME}}Editor::sendToWeb(const juce::String& type, const juce::var& data)
{
    if (!webView)
        return;

    juce::DynamicObject::Ptr message = new juce::DynamicObject();
    message->setProperty("type", type);
    message->setProperty("data", data);

    auto jsonString = juce::JSON::toString(message.get(), true);

    // Escape for JavaScript string
    jsonString = jsonString.replace("\\", "\\\\")
                           .replace("'", "\\'")
                           .replace("\n", "\\n")
                           .replace("\r", "\\r");

    webView->evaluateJavascript(
        "window.postMessage('" + jsonString + "', '*');",
        nullptr
    );
}

void {{PLUGIN_NAME}}Editor::sendParameterState()
{
    juce::DynamicObject::Ptr params = new juce::DynamicObject();

    for (auto* param : processorRef.getAPVTS().processor.getParameters())
    {
        if (auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*>(param))
        {
            params->setProperty(
                rangedParam->getParameterID(),
                rangedParam->convertFrom0to1(rangedParam->getValue())
            );
        }
    }

    sendToWeb("parameterState", params.get());
}

void {{PLUGIN_NAME}}Editor::sendSingleParameter(const juce::String& paramId, float value)
{
    juce::DynamicObject::Ptr data = new juce::DynamicObject();
    data->setProperty("id", paramId);
    data->setProperty("value", value);

    sendToWeb("parameterChanged", data.get());
}

//==============================================================================
void {{PLUGIN_NAME}}Editor::parameterChanged(const juce::String& parameterID, float newValue)
{
    // Called on audio thread - use async message to update UI
    juce::MessageManager::callAsync([this, parameterID, newValue]() {
        sendSingleParameter(parameterID, newValue);
    });
}
