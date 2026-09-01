import React, { useState } from 'react';
import { AudioSettingsView } from './components/AudioSettingsView';
import { MixerConsoleView } from './components/MixerConsoleView';
import { ArchitectureTestViewer } from './components/ArchitectureTestViewer';

export default function App() {
  const [activeTab, setActiveTab] = useState<'mixer' | 'settings'>('mixer');

  return (
    <div className="min-h-screen bg-zinc-950 text-zinc-100 flex flex-col justify-start items-center p-4 sm:p-8 space-y-6 antialiased selection:bg-emerald-600 selection:text-white">
      {/* App Header Banner */}
      <div className="w-full max-w-4xl flex items-center justify-between px-1 text-xs text-zinc-400">
        <div className="flex items-center space-x-2">
          <span className="w-2 h-2 rounded-full bg-emerald-500"></span>
          <span className="font-mono font-medium text-zinc-300">
            Live Mixer • Windows Desktop C++ / JUCE Stage 2 Architecture
          </span>
        </div>
        <div className="font-mono text-zinc-500">
          Target: x64 Windows / CMake / ASIO
        </div>
      </div>

      {/* Top View Selector Tabs */}
      <div className="w-full max-w-4xl flex items-center space-x-2 border-b border-zinc-800 pb-2">
        <button
          onClick={() => setActiveTab('mixer')}
          className={`px-4 py-1.5 text-xs font-bold rounded transition-colors ${
            activeTab === 'mixer'
              ? 'bg-zinc-800 text-white shadow-sm'
              : 'bg-zinc-950 text-zinc-400 hover:text-zinc-200'
          }`}
        >
          MIXER CONSOLE
        </button>
        <button
          onClick={() => setActiveTab('settings')}
          className={`px-4 py-1.5 text-xs font-bold rounded transition-colors ${
            activeTab === 'settings'
              ? 'bg-zinc-800 text-white shadow-sm'
              : 'bg-zinc-950 text-zinc-400 hover:text-zinc-200'
          }`}
        >
          AUDIO SETTINGS & ASIO
        </button>
      </div>

      {/* Main View Display */}
      {activeTab === 'mixer' ? <MixerConsoleView /> : <AudioSettingsView />}

      {/* C++ JUCE Architecture & Automated Test Runner */}
      <div className="w-full max-w-4xl">
        <ArchitectureTestViewer />
      </div>
    </div>
  );
}
