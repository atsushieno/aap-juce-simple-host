#include <iostream>
#include <memory>
//#include <JuceHeader.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#if JUCEAAP_ENABLED
#include <juceaap_audio_processors/juceaap_audio_plugin_format.h>
#endif

#ifndef APPLICATION_NAME
#define APPLICATION_NAME "AndroidPluginHost"
#endif
#ifndef APPLICATION_VERSION
#define APPLICATION_VERSION "0.1.0"
#endif
#define SETTINGS_PLUGIN_LIST "plugin-list"

using namespace juce;

class AppModel {
    ApplicationProperties settings{};
    AudioDeviceManager audioDeviceManager{};
    KnownPluginList knownPluginList{};
    AudioPluginFormatManager pluginFormatManager{};
    PluginDescription selectedPlugin{};
#if JUCEAAP_ENABLED
    std::unique_ptr<juceaap::AndroidAudioPluginFormat> androidAudioPluginFormat{nullptr};
#endif
    AudioProcessorGraph graph{};
    AudioProcessorPlayer player{};
    AudioProcessorGraph::Node::Ptr audioInputNode{nullptr}, audioOutputNode{nullptr}, midiInputNode{nullptr}, midiOutputNode{nullptr};

public:
    AppModel() {
#if JUCEAAP_ENABLED
        androidAudioPluginFormat = std::make_unique<juceaap::AndroidAudioPluginFormat>();
#endif
        pluginFormatManager.addDefaultFormats();
#if ANDROID
        pluginFormatManager.addFormat(getAndroidAudioPluginFormat());
#endif

        PropertiesFile::Options options{};
        options.osxLibrarySubFolder = "Application Support"; // It is super awkward that it has to be set like this. https://github.com/juce-framework/JUCE/blob/69795dc8e589a9eb5df251b6dd994859bf7b3fab/modules/juce_data_structures/app_properties/juce_PropertiesFile.cpp#L64
        options.applicationName = APPLICATION_NAME;
        options.storageFormat = PropertiesFile::StorageFormat::storeAsXML;
        settings.setStorageParameters(options);
        auto pluginList = settings.getUserSettings()->getXmlValue(SETTINGS_PLUGIN_LIST);

        if (pluginList)
            knownPluginList.recreateFromXml(*pluginList);

        audioDeviceManager.initialiseWithDefaultDevices(0, 2);
        audioDeviceManager.addAudioCallback(&player);

        if (RuntimePermissions::isGranted (RuntimePermissions::recordAudio))
            audioInputNode = graph.addNode(std::make_unique<AudioProcessorGraph::AudioGraphIOProcessor>(AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode));
        midiInputNode = graph.addNode(std::make_unique<AudioProcessorGraph::AudioGraphIOProcessor>(AudioProcessorGraph::AudioGraphIOProcessor::midiInputNode));
        audioOutputNode = graph.addNode(std::make_unique<AudioProcessorGraph::AudioGraphIOProcessor>(AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode));
        midiOutputNode = graph.addNode(std::make_unique<AudioProcessorGraph::AudioGraphIOProcessor>(AudioProcessorGraph::AudioGraphIOProcessor::midiOutputNode));

        graph.enableAllBuses();
        player.setProcessor(&graph);
    }

    ~AppModel() {
        audioDeviceManager.removeAudioCallback(&player);
    }

    AudioDeviceManager& getAudioDeviceManager() { return audioDeviceManager; }
    AudioPluginFormatManager& getPluginFormatManager() { return pluginFormatManager; }
    KnownPluginList& getKnownPluginList() { return knownPluginList; }
    AudioProcessorPlayer& getPluginPlayer() { return player; }

    void scanPlugins(AudioPluginFormat* format) {
        OwnedArray<PluginDescription> pluginDescriptions{};

        auto& list = getKnownPluginList();
        for (auto pd : list.getTypesForFormat(*format))
            list.removeType(pd);

        auto searchPaths = format->getDefaultLocationsToSearch();
        auto pluginFiles = format->searchPathsForPlugins(searchPaths, true);
        for (auto& pluginFile : pluginFiles) {
            printf("FileOrIdentifier: %s :\n", pluginFile.toRawUTF8());
            try {
                if (!list.getBlacklistedFiles().contains(pluginFile))
                    format->findAllTypesForFile(pluginDescriptions, pluginFile);
            } catch(std::runtime_error& ex) {
                // swallow scanner failures
                list.addToBlacklist(pluginFile);
                saveKnownPluginList();
            }
        }

        for (auto pluginDescription : pluginDescriptions)
            if (pluginDescription->pluginFormatName == format->getName())
                getKnownPluginList().addType(*pluginDescription);

        saveKnownPluginList();
    }

    void saveKnownPluginList() {
        settings.getUserSettings()->setValue(SETTINGS_PLUGIN_LIST, knownPluginList.createXml().get());
        settings.saveIfNeeded();
    }

    PluginDescription& getSelectedPlugin() { return selectedPlugin; }
    void setSelectedPlugin(PluginDescription plugin) {
        selectedPlugin = plugin;
    }

#if JUCEAAP_ENABLED
    juceaap::AndroidAudioPluginFormat* getAndroidAudioPluginFormat() {
        return androidAudioPluginFormat.get();
    }
#endif

    AudioProcessorGraph::Node::Ptr addActiveInstance(std::unique_ptr<AudioPluginInstance> instance) {
        auto node = graph.addNode(std::move(instance));
        updateGraph();
        return node;
    }

    void removeActiveInstance(AudioProcessorGraph::Node::Ptr node) {
        graph.removeNode(node->nodeID);
        updateGraph();
    }

    Array<AudioProcessorGraph::Node::Ptr> getActivePlugins() {
        Array<AudioProcessorGraph::Node::Ptr> ret{};
        for (auto node : graph.getNodes())
            if ((audioInputNode == nullptr || node->nodeID != audioInputNode->nodeID) &&
                node->nodeID != audioOutputNode->nodeID &&
                node->nodeID != midiInputNode->nodeID &&
                node->nodeID != midiOutputNode->nodeID)
                ret.add(node);
        return ret;
    }

    void updateGraph() {
        for (auto node : graph.getNodes())
            graph.disconnectNode(node->nodeID);
        juce::ReferenceCountedArray<AudioProcessorGraph::Node> plugins;
        for (auto node : graph.getNodes()) {
            if ((audioInputNode != nullptr && node->nodeID == audioInputNode->nodeID) ||
                node->nodeID == audioOutputNode->nodeID ||
                node->nodeID == midiInputNode->nodeID ||
                node->nodeID == midiOutputNode->nodeID)
                continue;
            plugins.add(node);
            node->getProcessor()->setPlayConfigDetails(graph.getMainBusNumInputChannels(),
                                                       graph.getMainBusNumOutputChannels(),
                                                       graph.getSampleRate(),
                                                       graph.getBlockSize());
        }
        auto prev = audioInputNode;
        for (auto node : plugins) {
            for (int channel = 0; channel < 2; ++channel) {
                if (prev != nullptr)
                    graph.addConnection ({ { prev->nodeID, channel },
                                           { node->nodeID, channel } });
            }
            prev = node;
        }
        for (int channel = 0; channel < 2; ++channel) {
            graph.addConnection ({ { prev->nodeID, channel },
                                   { audioOutputNode->nodeID, channel } });
        }

        graph.addConnection ({ { midiInputNode->nodeID,  juce::AudioProcessorGraph::midiChannelIndex },
                                        { midiOutputNode->nodeID, juce::AudioProcessorGraph::midiChannelIndex } });

        for (auto node : graph.getNodes())
            node->getProcessor()->enableAllBuses();
    }
};

std::unique_ptr<AppModel> appModel{};
AppModel* getAppModel() {
    if (!appModel)
        appModel = std::make_unique<AppModel>();
    return appModel.get();
}

class MainComponent : public Component {
    const char* allVendors = "--- All Vendors ---";
    const char* unnamedVendor = "--- (No Name) ---";

    TextButton buttonScanPlugins{"Scan plugins"};
    TextButton buttonShowAudioSettings{"Audio Settings"};
    ComboBox comboBoxPluginFormats{};
    ComboBox comboBoxPluginVendors{};
    ComboBox comboBoxPlugins{};
    ComboBox comboBoxActivePlugins{};
    TextButton buttonAddPlugin{"Add"};
    TextButton buttonRemoveActivePlugin{"Remove"};
    TextButton buttonShowUI{"Show UI"};

    OwnedArray<DocumentWindow> pluginWindows{};

    Label labelStatusText{};

    ToggleButton mpeToggle{"MPE"};
    MidiKeyboardState midiKeyboardState;
    MidiKeyboardComponent midiKeyboard{midiKeyboardState, KeyboardComponentBase::Orientation::horizontalKeyboard};
    MPEZoneLayout mpeZoneLayout{MPEZone{MPEZone::Type::lower, 7}, MPEZone{MPEZone::Type::upper, 7}};
    MPEInstrument mpeInstrument{mpeZoneLayout};
    MPEKeyboardComponent mpeKeyboard{mpeInstrument, KeyboardComponentBase::Orientation::horizontalKeyboard};

    TextButton buttonSetupMidiInDevices{"Setup MIDI In"};
    TextButton buttonSetupMidiOutDevices{"Setup MIDI Out"};
    ComboBox comboBoxMidiInDevices{};
    ComboBox comboBoxMidiOutDevices{};

    class AudioPluginFormatComparer {
    public:
        int compareElements (AudioPluginFormat* first, AudioPluginFormat* second)
        {
            return first->getName().compare(second->getName());
        }
    };

    class PluginWindow : public DocumentWindow {
        MainComponent* owner;
    public:
        PluginWindow(MainComponent *mainComponent, AudioProcessor* processor)
        : DocumentWindow(processor->getName(), Colours::black, DocumentWindow::TitleBarButtons::allButtons),
        owner(mainComponent) {}

        void closeButtonPressed() override {
            owner->closePluginWindow(this);
        }
    };

public:
    MainComponent() {
        auto& formatManager = getAppModel()->getPluginFormatManager();

        // trivial formats first, then non-trivial formats follow.
        auto formats = getPluginFormats();
        for (auto format : formats)
            if (format->isTrivialToScan())
                comboBoxPluginFormats.addItem(format->getName(), comboBoxPluginFormats.getNumItems() + 1);
        for (auto format : formatManager.getFormats())
            if (!format->isTrivialToScan())
                comboBoxPluginFormats.addItem(format->getName(), comboBoxPluginFormats.getNumItems() + 1);
        comboBoxPluginFormats.setSelectedId(1);

        comboBoxPluginFormats.onChange = [&] {
            updatePluginVendorListOnUI();
            updatePluginListOnUI();
        };

        buttonScanPlugins.onClick = [this, formats] {
            appModel->scanPlugins(formats[comboBoxPluginFormats.getSelectedId() - 1]);
            updatePluginVendorListOnUI();
            updatePluginListOnUI();
        };

        buttonShowAudioSettings.onClick = [&] {
            auto settings = new AudioDeviceSelectorComponent(appModel->getAudioDeviceManager(),
                                                         0, 256, 2, 256,
                                                         true, true, true, false);
            settings->setBounds(0, 0, 400, 400);
            class Window : public DocumentWindow {
            public:
                Window(String title, Colour backgroundColor, int buttons)
                : DocumentWindow(title, backgroundColor, buttons) {}

                void closeButtonPressed() override {
                    delete this;
                }
            };
            auto window = new Window("Audio settings", Colours::black, DocumentWindow::allButtons);
            window->setContentOwned(settings, true);
            window->setVisible(true);
        };

        comboBoxPluginVendors.onChange = [&] {
            updatePluginListOnUI();
        };

        comboBoxPlugins.onChange = [&] {
            bool filtered = comboBoxPluginVendors.getSelectedId() > 1;
            auto vendor = comboBoxPluginVendors.getText();
            for (auto& desc : appModel->getKnownPluginList().getTypes()) {
                if (!filtered || desc.manufacturerName == vendor || desc.manufacturerName.isEmpty() && vendor == unnamedVendor) {
                    if (desc.descriptiveName == comboBoxPlugins.getText()) {
                        appModel->setSelectedPlugin(desc);
                        return;
                    }
                }
            }
        };

        buttonAddPlugin.onClick = [&] {
            auto& desc = appModel->getSelectedPlugin();
            if (desc.uniqueId != 0) {
                for (auto format : formatManager.getFormats()) {
                    if (format->getName() != comboBoxPluginFormats.getText())
                        continue;
                    format->createPluginInstanceAsync(desc, 44100, 1024 * 32, [&](std::unique_ptr<AudioPluginInstance> instance, String error) {
                        if (error.isEmpty()) {
                            comboBoxActivePlugins.addItem(instance->getName(), comboBoxActivePlugins.getNumItems() + 1);
                            comboBoxActivePlugins.setSelectedId(comboBoxActivePlugins.getNumItems(), NotificationType::sendNotificationAsync);
                            auto node = appModel->addActiveInstance(std::move(instance));
                            showPluginUI(node);
                        } else {
                            AlertWindow::showMessageBoxAsync(MessageBoxIconType::WarningIcon, "Plugin Error", error);
                        }
                    });
                }
            }
        };

        buttonRemoveActivePlugin.onClick = [&] {
            AlertWindow::showMessageBoxAsync(MessageBoxIconType::WarningIcon,
                                             "Not supported",
                                             "Unfortunately, we cannot remove an item from JUCE ComboBox because there is no API for that. Therefore this operation is not supported.");
            /*
            auto node = getSelectedActivePlugin();
            if (node) {
                appModel->removeActiveInstance(node);
                updatePluginListOnUI();
            }*/
        };

        buttonShowUI.onClick = [&] {
            auto node = getSelectedActivePlugin();
            if (node)
                showPluginUI(node);
        };

        // setup components
        comboBoxPluginFormats.setBounds(0, 0, 100, 50);
        buttonScanPlugins.setBounds(0, 50, 150, 50);
        buttonShowAudioSettings.setBounds(200, 50, 150, 50);
        comboBoxPluginVendors.setBounds(0, 100, 400, 50);
        comboBoxPlugins.setBounds(0, 150, 400, 50);
        buttonAddPlugin.setBounds(0, 200, 150, 50);
        comboBoxActivePlugins.setBounds(0, 250, 400, 50);
        buttonRemoveActivePlugin.setBounds(0, 300, 150, 50);
        buttonShowUI.setBounds(200, 300, 150, 50);
        labelStatusText.setBounds(0, 350, 200, 50);

        addAndMakeVisible(comboBoxPluginFormats);
        addAndMakeVisible(buttonScanPlugins);
        addAndMakeVisible(buttonShowAudioSettings);
        addAndMakeVisible(comboBoxPluginVendors);
        addAndMakeVisible(comboBoxPlugins);
        addAndMakeVisible(buttonAddPlugin);
        addAndMakeVisible(comboBoxActivePlugins);
        addAndMakeVisible(buttonRemoveActivePlugin);
        addAndMakeVisible(buttonShowUI);
        addAndMakeVisible(labelStatusText);
        labelStatusText.setText("Ready", NotificationType::sendNotificationAsync);

        updatePluginVendorListOnUI();
        updatePluginListOnUI();

        // Set MIDI/MPE keyboard
        midiKeyboardState.addListener(&appModel->getPluginPlayer().getMidiMessageCollector());
        mpeToggle.setBounds(0, 400, 400, 50);
        mpeToggle.onClick = [&] {
            if (midiKeyboard.isVisible())
                mpeKeyboard.setLowestVisibleKey(midiKeyboard.getLowestVisibleKey());
            else
                midiKeyboard.setLowestVisibleKey(mpeKeyboard.getLowestVisibleKey());
            midiKeyboard.setVisible(!midiKeyboard.isVisible());
            mpeKeyboard.setVisible(!mpeKeyboard.isVisible());
        };
        addAndMakeVisible(mpeToggle);
        midiKeyboard.setBounds(0, 450, 400, 50);
        addAndMakeVisible(midiKeyboard);
        mpeInstrument.enableLegacyMode();
        mpeInstrument.setZoneLayout(mpeZoneLayout);
        mpeKeyboard.setBounds(0, 450, 400, 50);
        addAndMakeVisible(mpeKeyboard);
        mpeKeyboard.setVisible(false);

        /*
        // Setup MIDI devices playgound
        buttonSetupMidiInDevices.onClick = [&] {
            comboBoxMidiInDevices.clear(NotificationType::sendNotificationAsync);
            for (auto midiDevice : juce::MidiInput::getAvailableDevices())
                comboBoxMidiInDevices.addItem(midiDevice.name, comboBoxMidiInDevices.getNumItems() + 1);
        };
        buttonSetupMidiOutDevices.onClick = [&] {
            comboBoxMidiOutDevices.clear(NotificationType::sendNotificationAsync);
            auto defaultOut = juce::MidiOutput::getDefaultDevice();
            for (auto midiDevice : juce::MidiOutput::getAvailableDevices())
                comboBoxMidiOutDevices.addItem(midiDevice.name, comboBoxMidiOutDevices.getNumItems() + 1);
        };

        buttonSetupMidiInDevices.setBounds(0, 300, 150, 50);
        buttonSetupMidiOutDevices.setBounds(0, 350, 150, 50);
        comboBoxMidiInDevices.setBounds(200, 300, 200, 50);
        comboBoxMidiOutDevices.setBounds(200, 350, 200, 50);
        addAndMakeVisible(buttonSetupMidiInDevices);
        addAndMakeVisible(buttonSetupMidiOutDevices);
        addAndMakeVisible(comboBoxMidiInDevices);
        addAndMakeVisible(comboBoxMidiOutDevices);
        */
    }

    ~MainComponent() {

    }

    void closePluginWindow(PluginWindow *window) {
        pluginWindows.removeObject(window);
    }

    juce::Array<AudioPluginFormat*> getPluginFormats() {
        auto& formatManager = appModel->getPluginFormatManager();
        juce::Array<AudioPluginFormat*> formats{formatManager.getFormats()};
        AudioPluginFormatComparer cmp{};
        formats.sort<AudioPluginFormatComparer>(cmp);
        return formats;
    }

    void updatePluginVendorListOnUI() {
        if (appModel->getKnownPluginList().getNumTypes() == 0)
            return;
        auto format = getPluginFormats()[comboBoxPluginFormats.getSelectedId() - 1];
        Array<String> vendors{};
        for (auto &desc: appModel->getKnownPluginList().getTypesForFormat(*format)) {
            if (!vendors.contains(desc.manufacturerName))
                vendors.add(desc.manufacturerName);
        }
        comboBoxPluginVendors.clear(NotificationType::dontSendNotification);
        comboBoxPluginVendors.addItem(allVendors, 1);
        comboBoxPluginVendors.addItem(unnamedVendor, 2);
        for (auto &vendor : vendors)
            if (vendor.isNotEmpty()) // sometimes it is empty
                comboBoxPluginVendors.addItem(vendor, comboBoxPluginVendors.getNumItems() + 2);
        comboBoxPluginVendors.setSelectedId(1, NotificationType::sendNotificationAsync);
    }

    void updatePluginListOnUI() {
        auto format = getPluginFormats()[comboBoxPluginFormats.getSelectedItemIndex()];
        bool filtered = comboBoxPluginVendors.getSelectedItemIndex() > 0;
        auto vendor = comboBoxPluginVendors.getText();
        Array<PluginDescription> plugins{};
        for (auto& desc : appModel->getKnownPluginList().getTypesForFormat(*format))
            if (!filtered || desc.manufacturerName == vendor || desc.manufacturerName.isEmpty() && vendor == unnamedVendor)
                plugins.add(desc);
        comboBoxPlugins.clear(NotificationType::sendNotificationAsync);
        for (auto& desc : plugins)
            comboBoxPlugins.addItem(desc.descriptiveName, comboBoxPlugins.getNumItems() + 1);
    }

    void showPluginUI(AudioProcessorGraph::Node::Ptr node) {
        if (node->getProcessor()->hasEditor()) {
            auto editor = node->getProcessor()->createEditorIfNeeded();
            auto window = new PluginWindow(this, node->getProcessor());
            pluginWindows.add(window);
            window->setBounds(editor->getBounds());
            window->setContentOwned(editor, true);
            window->setVisible(true);
        }
    }

    AudioProcessorGraph::Node::Ptr getSelectedActivePlugin() {
        auto index = comboBoxActivePlugins.getSelectedItemIndex();
        if (index >= 0)
            return appModel->getActivePlugins()[index];
        return nullptr;
    }
};

class AndroidPluginHostApplication  : public JUCEApplication
{
public:
    //==============================================================================
    AndroidPluginHostApplication() = default;

    ~AndroidPluginHostApplication() override {
        appModel.reset(nullptr);
    }

    const String getApplicationName() override       { return APPLICATION_NAME; }
    const String getApplicationVersion() override    { return APPLICATION_VERSION; }
    bool moreThanOneInstanceAllowed() override       { return true; }

    //==============================================================================
    void initialise (const String& commandLine) override
    {
        mainWindow = std::make_unique<MainAppWindow> (getApplicationName());


    }

    bool backButtonPressed() override    { return true; }
    void shutdown() override             { mainWindow = nullptr; }

    //==============================================================================
    void systemRequestedQuit() override                   { quit(); }
    void anotherInstanceStarted (const String&) override  {}

    ApplicationCommandManager& getGlobalCommandManager()  { return commandManager; }

private:
    class MainAppWindow    : public DocumentWindow
    {
    public:
        explicit MainAppWindow (const String& name)
                : DocumentWindow (name, Desktop::getInstance().getDefaultLookAndFeel()
                                          .findColour (ResizableWindow::backgroundColourId),
                                  DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setResizable (true, false);
            setResizeLimits (400, 400, 10000, 10000);

#if JUCE_IOS || JUCE_ANDROID
            setFullScreen (true);

            auto& desktop = Desktop::getInstance();

            desktop.setOrientationsEnabled (Desktop::allOrientations);
            desktop.setKioskModeComponent (this);
#else
            setBounds ((int) (0.1f * (float) getParentWidth()),
                       (int) (0.1f * (float) getParentHeight()),
                       jmax (850, (int) (0.5f * (float) getParentWidth())),
                       jmax (600, (int) (0.7f * (float) getParentHeight())));
#endif

            setContentOwned (new MainComponent(), false);
            setVisible (true);
        }

        void closeButtonPressed() override    { JUCEApplication::getInstance()->systemRequestedQuit(); }

#if JUCE_IOS || JUCE_ANDROID
        void parentSizeChanged() override
        {
            getMainComponent().resized();
        }
#endif

        //==============================================================================
        MainComponent& getMainComponent()    { return *dynamic_cast<MainComponent*> (getContentComponent()); }

    private:
        std::unique_ptr<Component> taskbarIcon;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainAppWindow)
    };

    std::unique_ptr<MainAppWindow> mainWindow;
    ApplicationCommandManager commandManager;
};

ApplicationCommandManager& getGlobalCommandManager()
{
    return dynamic_cast<AndroidPluginHostApplication*> (JUCEApplication::getInstance())->getGlobalCommandManager();
}

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION (AndroidPluginHostApplication)
