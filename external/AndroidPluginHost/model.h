#ifndef ANDROIDPLUGINHOST_MODEL_H
#define ANDROIDPLUGINHOST_MODEL_H

#include "audioplayer.h"
#include <juce_audio_processors/juce_audio_processors.h>

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
    AudioProcessorGraph::Node::Ptr audioInputNode{nullptr}, audioOutputNode{nullptr},
        midiInputNode{nullptr}, midiOutputNode{nullptr}, audioPlayerNode{nullptr};

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
        audioPlayerNode = graph.addNode(std::make_unique<AudioFilePlayerProcessor>());

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

    AudioFilePlayerProcessor* getAudioPlayer() {
        return dynamic_cast<AudioFilePlayerProcessor*>(audioPlayerNode->getProcessor());
    }

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
                node->nodeID != midiOutputNode->nodeID &&
                node->nodeID != audioPlayerNode->nodeID)
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
            node->getProcessor()->setPlayConfigDetails(graph.getMainBusNumInputChannels(),
                                                       graph.getMainBusNumOutputChannels(),
                                                       graph.getSampleRate(),
                                                       graph.getBlockSize());
            if (node->nodeID != audioPlayerNode->nodeID)
                plugins.add(node);
        }
        // FIXME: connect audioInputNode too.
        auto prev = audioPlayerNode;
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

        prev = midiInputNode;
        for (auto node : plugins) {
            if (node->getProcessor()->acceptsMidi())
                graph.addConnection ({ { prev->nodeID,  juce::AudioProcessorGraph::midiChannelIndex },
                                       { node->nodeID, juce::AudioProcessorGraph::midiChannelIndex } });
            if (node->getProcessor()->producesMidi())
                prev = node;
        }

        for (auto node : graph.getNodes())
            node->getProcessor()->enableAllBuses();
    }
};

#endif //ANDROIDPLUGINHOST_MODEL_H
