import React, { useState, useEffect } from 'react';
import {
  Volume2,
  Sliders,
  Play,
  Square,
  AlertTriangle,
  Radio,
  Zap,
  Activity,
  Mic,
  Music,
  Maximize2,
  Minus,
  X,
} from 'lucide-react';
import { DriverType, AudioState, AudioMetrics } from '../types';
import { globalAudioEngine, DEFAULT_DEVICES } from '../audioEngineSim';
import { AsioControlPanelModal } from './AsioControlPanelModal';

export const AudioSettingsView: React.FC = () => {
  const [config, setConfig] = useState(globalAudioEngine.getConfig());
  const [metrics, setMetrics] = useState<AudioMetrics>(globalAudioEngine.getMetrics());
  const [levels, setLevels] = useState({ inLeft: 0, inRight: 0, outLeft: 0, outRight: 0 });
  const [isAsioModalOpen, setIsAsioModalOpen] = useState(false);
  const [statusMessage, setStatusMessage] = useState<string | null>(null);
  const [testSignalType, setTestSignalType] = useState<'mic' | 'oscillator' | 'silence'>('oscillator');

  useEffect(() => {
    const interval = setInterval(() => {
      setMetrics(globalAudioEngine.getMetrics());
      setLevels(globalAudioEngine.getLevels());
    }, 33); // 30 FPS polling matching JUCE Timer

    globalAudioEngine.onDisconnect((msg) => {
      setStatusMessage(msg);
    });

    return () => clearInterval(interval);
  }, []);

  const handleDriverChange = (driver: DriverType) => {
    globalAudioEngine.setDriver(driver);
    setConfig(globalAudioEngine.getConfig());
    setStatusMessage(null);
  };

  const handleDeviceChange = (deviceName: string) => {
    globalAudioEngine.setDevice(deviceName);
    setConfig(globalAudioEngine.getConfig());
    setStatusMessage(null);
  };

  const handleSampleRateChange = (rate: number) => {
    globalAudioEngine.setSampleRate(rate);
    setConfig(globalAudioEngine.getConfig());
  };

  const handleBufferSizeChange = (size: number) => {
    globalAudioEngine.setBufferSize(size);
    setConfig(globalAudioEngine.getConfig());
  };

  const handleStartAudio = async () => {
    setStatusMessage(null);
    await globalAudioEngine.start();
    setMetrics(globalAudioEngine.getMetrics());
  };

  const handleStopAudio = () => {
    globalAudioEngine.stop();
    setMetrics(globalAudioEngine.getMetrics());
  };

  const handleSimulateDisconnect = () => {
    globalAudioEngine.simulateDeviceDisconnect();
    setMetrics(globalAudioEngine.getMetrics());
  };

  const handleInjectXRun = () => {
    globalAudioEngine.triggerSimulatedXRun();
    setMetrics(globalAudioEngine.getMetrics());
  };

  const handleSignalChange = (type: 'mic' | 'oscillator' | 'silence') => {
    setTestSignalType(type);
    globalAudioEngine.setTestSignalType(type);
  };

  const currentDevice = DEFAULT_DEVICES.find((d) => d.name === config.deviceName);
  const availableDevices = globalAudioEngine.getAvailableDevices(config.driverType);
  const supportedSampleRates = currentDevice ? currentDevice.supportedSampleRates : [44100, 48000, 96000];
  const supportedBufferSizes = currentDevice ? currentDevice.supportedBufferSizes : [64, 128, 256, 512];

  // Audio status indicator styling
  const getStatusBadge = (state: AudioState) => {
    switch (state) {
      case 'Running':
        return {
          dotColor: 'bg-emerald-500 animate-pulse',
          textColor: 'text-emerald-400 font-bold',
          label: '● RUNNING',
        };
      case 'Ready':
        return {
          dotColor: 'bg-blue-500',
          textColor: 'text-blue-400 font-bold',
          label: '● READY',
        };
      case 'Initializing':
        return {
          dotColor: 'bg-amber-500 animate-pulse',
          textColor: 'text-amber-400 font-bold',
          label: '● INITIALIZING',
        };
      case 'Stopping':
        return {
          dotColor: 'bg-amber-500',
          textColor: 'text-amber-400 font-bold',
          label: '● STOPPING',
        };
      case 'Error':
        return {
          dotColor: 'bg-rose-500',
          textColor: 'text-rose-400 font-bold',
          label: '● ERROR',
        };
      case 'Recovering':
        return {
          dotColor: 'bg-orange-500 animate-pulse',
          textColor: 'text-orange-400 font-bold',
          label: '● RECOVERING',
        };
      default:
        return {
          dotColor: 'bg-zinc-500',
          textColor: 'text-zinc-400 font-bold',
          label: '● OFFLINE',
        };
    }
  };

  const statusBadge = getStatusBadge(metrics.audioState);

  return (
    <div className="w-full max-w-2xl mx-auto bg-zinc-900 border border-zinc-700/80 rounded-lg shadow-2xl overflow-hidden font-sans text-zinc-100">
      {/* Windows Native-style Title Bar */}
      <div className="flex items-center justify-between px-3.5 py-2 bg-zinc-950 border-b border-zinc-800 select-none">
        <div className="flex items-center space-x-2">
          <Activity className="w-4 h-4 text-emerald-400" />
          <span className="text-xs font-semibold tracking-wide text-zinc-200">
            Live Mixer - Desktop Audio Configuration (v0.1 Foundation)
          </span>
        </div>
        <div className="flex items-center space-x-2 text-zinc-400">
          <button className="p-1 hover:bg-zinc-800 rounded">
            <Minus className="w-3.5 h-3.5" />
          </button>
          <button className="p-1 hover:bg-zinc-800 rounded">
            <Maximize2 className="w-3 h-3" />
          </button>
          <button className="p-1 hover:bg-rose-900 hover:text-white rounded">
            <X className="w-3.5 h-3.5" />
          </button>
        </div>
      </div>

      {/* Main Container */}
      <div className="p-6 space-y-6">
        {/* Title Header */}
        <div className="text-center pb-2 border-b border-zinc-800">
          <h1 className="text-xl font-bold tracking-widest text-zinc-100 uppercase">
            LIVE MIXER
          </h1>
          <p className="text-xs text-zinc-400 mt-0.5">
            Realtime Low-Latency Audio Engine & Device Layer
          </p>
        </div>

        {/* Status Alert if Disconnected */}
        {statusMessage && (
          <div className="p-3 bg-rose-950/60 border border-rose-800 text-rose-300 text-xs rounded flex items-center justify-between animate-in fade-in">
            <div className="flex items-center space-x-2">
              <AlertTriangle className="w-4 h-4 text-rose-400 shrink-0" />
              <span>{statusMessage}</span>
            </div>
            <button
              onClick={() => setStatusMessage(null)}
              className="text-xs underline text-rose-300 hover:text-white"
            >
              Dismiss
            </button>
          </div>
        )}

        {/* Configuration Controls (Form Grid) */}
        <div className="space-y-3.5 bg-zinc-950/60 p-4 rounded-lg border border-zinc-800">
          {/* Driver Row */}
          <div className="grid grid-cols-12 items-center gap-3">
            <label className="col-span-4 text-xs font-semibold text-zinc-300">
              Driver:
            </label>
            <div className="col-span-8">
              <select
                value={config.driverType}
                onChange={(e) => handleDriverChange(e.target.value as DriverType)}
                className="w-full bg-zinc-900 border border-zinc-700 hover:border-zinc-500 text-zinc-100 rounded px-3 py-1.5 text-xs font-mono focus:border-emerald-500 focus:outline-hidden transition-colors"
              >
                <option value="ASIO">ASIO (Low Latency Steinberg Audio)</option>
                <option value="WASAPI">WASAPI (Windows Audio Session API)</option>
              </select>
            </div>
          </div>

          {/* Device Row */}
          <div className="grid grid-cols-12 items-center gap-3">
            <label className="col-span-4 text-xs font-semibold text-zinc-300">
              Device:
            </label>
            <div className="col-span-8">
              <select
                value={config.deviceName}
                onChange={(e) => handleDeviceChange(e.target.value)}
                className="w-full bg-zinc-900 border border-zinc-700 hover:border-zinc-500 text-zinc-100 rounded px-3 py-1.5 text-xs font-mono focus:border-emerald-500 focus:outline-hidden transition-colors"
              >
                {availableDevices.map((dev) => (
                  <option key={dev.id} value={dev.name}>
                    {dev.name} {dev.isDefault ? '(Default)' : ''}
                  </option>
                ))}
              </select>
            </div>
          </div>

          {/* Sample Rate Row */}
          <div className="grid grid-cols-12 items-center gap-3">
            <label className="col-span-4 text-xs font-semibold text-zinc-300">
              Sample Rate:
            </label>
            <div className="col-span-8">
              <select
                value={config.sampleRate}
                onChange={(e) => handleSampleRateChange(Number(e.target.value))}
                className="w-full bg-zinc-900 border border-zinc-700 hover:border-zinc-500 text-zinc-100 rounded px-3 py-1.5 text-xs font-mono focus:border-emerald-500 focus:outline-hidden transition-colors"
              >
                {supportedSampleRates.map((rate) => (
                  <option key={rate} value={rate}>
                    {rate} Hz {rate === 48000 ? '(Default)' : ''}
                  </option>
                ))}
              </select>
            </div>
          </div>

          {/* Buffer Size Row */}
          <div className="grid grid-cols-12 items-center gap-3">
            <label className="col-span-4 text-xs font-semibold text-zinc-300">
              Buffer:
            </label>
            <div className="col-span-8">
              <select
                value={config.bufferSize}
                onChange={(e) => handleBufferSizeChange(Number(e.target.value))}
                className="w-full bg-zinc-900 border border-zinc-700 hover:border-zinc-500 text-zinc-100 rounded px-3 py-1.5 text-xs font-mono focus:border-emerald-500 focus:outline-hidden transition-colors"
              >
                {supportedBufferSizes.map((size) => (
                  <option key={size} value={size}>
                    {size} samples (~{((size / config.sampleRate) * 1000).toFixed(2)} ms)
                    {size === 128 ? ' (Default)' : ''}
                  </option>
                ))}
              </select>
            </div>
          </div>

          {/* Channels Row */}
          <div className="grid grid-cols-12 items-center gap-3 pt-2 border-t border-zinc-800/80">
            <div className="col-span-6 flex items-center justify-between pr-3 border-r border-zinc-800">
              <span className="text-xs text-zinc-400">Input Channels:</span>
              <span className="text-xs font-mono font-bold text-zinc-200">
                {config.inputChannelCount}
              </span>
            </div>
            <div className="col-span-6 flex items-center justify-between pl-3">
              <span className="text-xs text-zinc-400">Output Channels:</span>
              <span className="text-xs font-mono font-bold text-zinc-200">
                {config.outputChannelCount}
              </span>
            </div>
          </div>

          {/* Audio Status Row */}
          <div className="grid grid-cols-12 items-center gap-3 pt-2 border-t border-zinc-800/80">
            <label className="col-span-4 text-xs font-semibold text-zinc-300">
              Audio Status:
            </label>
            <div className="col-span-8 flex items-center space-x-2">
              <span className={`text-xs font-mono ${statusBadge.textColor}`}>
                {statusBadge.label}
              </span>
            </div>
          </div>

          {/* Telemetry Metrics Grid */}
          <div className="grid grid-cols-2 gap-2.5 pt-2 border-t border-zinc-800/80 text-xs font-mono">
            <div className="col-span-2 flex items-center justify-between bg-zinc-900/80 px-2.5 py-1.5 rounded border border-zinc-800">
              <span className="text-zinc-400">Buffer Duration:</span>
              <span className="text-blue-400 font-semibold">
                {((config.bufferSize / config.sampleRate) * 1000).toFixed(2)} ms
              </span>
            </div>
            <div className="flex items-center justify-between bg-zinc-900/80 px-2.5 py-1.5 rounded border border-zinc-800">
              <span className="text-zinc-400">Input Latency:</span>
              <span className="text-emerald-400 font-semibold">
                {metrics.inputLatencyMs.toFixed(2)} ms
              </span>
            </div>
            <div className="flex items-center justify-between bg-zinc-900/80 px-2.5 py-1.5 rounded border border-zinc-800">
              <span className="text-zinc-400">Output Latency:</span>
              <span className="text-emerald-400 font-semibold">
                {metrics.outputLatencyMs.toFixed(2)} ms
              </span>
            </div>
            <div className="flex items-center justify-between bg-zinc-900/80 px-2.5 py-1.5 rounded border border-zinc-800">
              <span className="text-zinc-400">Processing:</span>
              <span className="text-zinc-200">
                {metrics.processingTimeMs.toFixed(2)} ms
              </span>
            </div>
            <div className="flex items-center justify-between bg-zinc-900/80 px-2.5 py-1.5 rounded border border-zinc-800">
              <span className="text-zinc-400">XRuns:</span>
              <span className={metrics.xrunCount > 0 ? 'text-amber-400 font-bold' : 'text-zinc-200'}>
                {metrics.xrunCount}
              </span>
            </div>
          </div>
        </div>

        {/* Start / Stop Buttons */}
        <div className="grid grid-cols-2 gap-3">
          <button
            onClick={handleStartAudio}
            disabled={metrics.audioState === 'Running'}
            className="flex items-center justify-center space-x-2 py-2.5 bg-emerald-600 hover:bg-emerald-500 disabled:opacity-40 disabled:hover:bg-emerald-600 text-white text-xs font-bold uppercase tracking-wider rounded shadow transition-all active:scale-98"
          >
            <Play className="w-4 h-4 fill-white" />
            <span>START AUDIO</span>
          </button>

          <button
            onClick={handleStopAudio}
            disabled={metrics.audioState !== 'Running'}
            className="flex items-center justify-center space-x-2 py-2.5 bg-rose-600 hover:bg-rose-500 disabled:opacity-40 disabled:hover:bg-rose-600 text-white text-xs font-bold uppercase tracking-wider rounded shadow transition-all active:scale-98"
          >
            <Square className="w-4 h-4 fill-white" />
            <span>STOP AUDIO</span>
          </button>
        </div>

        {/* ASIO Control Panel Button */}
        <div>
          <button
            onClick={() => setIsAsioModalOpen(true)}
            disabled={config.driverType !== 'ASIO'}
            className="w-full flex items-center justify-center space-x-2 py-2 bg-zinc-800 hover:bg-zinc-700 disabled:opacity-30 disabled:hover:bg-zinc-800 text-zinc-200 border border-zinc-700 text-xs font-semibold rounded transition-colors"
          >
            <Sliders className="w-3.5 h-3.5 text-emerald-400" />
            <span>Open ASIO Control Panel</span>
          </button>
        </div>

        {/* Realtime Passthrough Live VU Monitor */}
        <div className="p-3.5 bg-zinc-950/80 border border-zinc-800 rounded-lg space-y-2.5">
          <div className="flex items-center justify-between text-xs">
            <div className="flex items-center space-x-2">
              <Volume2 className="w-3.5 h-3.5 text-emerald-400" />
              <span className="font-semibold text-zinc-300">
                Hardware Passthrough Monitor (Stereo In 1/2 ➔ Out 1/2)
              </span>
            </div>
            <span className="text-[11px] font-mono text-zinc-500">
              No DSP • No Mixer • Direct Realtime Bus
            </span>
          </div>

          <div className="space-y-1.5 pt-1 font-mono text-[11px]">
            {/* Left Channel */}
            <div className="flex items-center space-x-2">
              <span className="w-8 text-zinc-400 text-right">In 1:</span>
              <div className="flex-1 h-3 bg-zinc-900 rounded-xs overflow-hidden flex">
                <div
                  className="h-full bg-emerald-500 transition-all duration-75"
                  style={{ width: `${Math.min(100, levels.inLeft * 120)}%` }}
                />
              </div>
              <span className="w-8 text-zinc-400 text-right">Out 1</span>
            </div>

            {/* Right Channel */}
            <div className="flex items-center space-x-2">
              <span className="w-8 text-zinc-400 text-right">In 2:</span>
              <div className="flex-1 h-3 bg-zinc-900 rounded-xs overflow-hidden flex">
                <div
                  className="h-full bg-emerald-500 transition-all duration-75"
                  style={{ width: `${Math.min(100, levels.inRight * 120)}%` }}
                />
              </div>
              <span className="w-8 text-zinc-400 text-right">Out 2</span>
            </div>
          </div>
        </div>

        {/* Diagnostic & Hardware Disconnect Simulation Controls */}
        <div className="p-3 bg-zinc-950/40 border border-zinc-800/80 rounded-lg space-y-2 text-xs">
          <div className="flex items-center justify-between">
            <span className="font-semibold text-zinc-400 uppercase tracking-wider text-[10px]">
              Hardware Testing & Signal Generator
            </span>
            <div className="flex items-center space-x-1">
              <button
                onClick={() => handleSignalChange('oscillator')}
                className={`px-2 py-0.5 rounded text-[11px] flex items-center space-x-1 ${
                  testSignalType === 'oscillator'
                    ? 'bg-emerald-950 text-emerald-300 border border-emerald-700'
                    : 'text-zinc-500 hover:text-zinc-300'
                }`}
              >
                <Music className="w-3 h-3" />
                <span>440Hz Tone</span>
              </button>
              <button
                onClick={() => handleSignalChange('mic')}
                className={`px-2 py-0.5 rounded text-[11px] flex items-center space-x-1 ${
                  testSignalType === 'mic'
                    ? 'bg-emerald-950 text-emerald-300 border border-emerald-700'
                    : 'text-zinc-500 hover:text-zinc-300'
                }`}
              >
                <Mic className="w-3 h-3" />
                <span>Live Mic In</span>
              </button>
            </div>
          </div>

          <div className="grid grid-cols-2 gap-2 pt-1">
            <button
              onClick={handleInjectXRun}
              className="py-1 px-2.5 bg-zinc-900 hover:bg-zinc-800 border border-zinc-700/70 text-zinc-300 rounded text-[11px] flex items-center justify-center space-x-1.5 transition-colors"
            >
              <Zap className="w-3 h-3 text-amber-400" />
              <span>Simulate Dropout / XRun</span>
            </button>
            <button
              onClick={handleSimulateDisconnect}
              className="py-1 px-2.5 bg-zinc-900 hover:bg-rose-950/50 border border-zinc-700/70 hover:border-rose-700 text-zinc-300 hover:text-rose-300 rounded text-[11px] flex items-center justify-center space-x-1.5 transition-colors"
            >
              <Radio className="w-3 h-3 text-rose-400" />
              <span>Simulate Device Disconnect</span>
            </button>
          </div>
        </div>
      </div>

      {/* ASIO Control Panel Modal */}
      <AsioControlPanelModal
        isOpen={isAsioModalOpen}
        onClose={() => setIsAsioModalOpen(false)}
        deviceName={config.deviceName}
        bufferSize={config.bufferSize}
        sampleRate={config.sampleRate}
        onBufferSizeChange={handleBufferSizeChange}
      />
    </div>
  );
};
