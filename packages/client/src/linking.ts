import type {
  LinkerCommandResult,
  LinkerRuntimeState,
  LinkerTarget,
  OnclawClient,
} from "./contracts";

export type StockLinkMode = "off" | "read-only" | "ths-to-tdx" | "tdx-to-ths";

export interface StockLinkRuntimeOptions {
  client: OnclawClient;
  mode?: StockLinkMode;
  pollIntervalMs?: number;
  onState?: (state: LinkerRuntimeState) => void;
  onCommand?: (result: LinkerCommandResult) => void;
  onError?: (error: unknown) => void;
}

export interface StockLinkRuntime {
  start: () => () => void;
  stop: () => void;
  pollNow: () => Promise<void>;
  setMode: (mode: StockLinkMode) => void;
  getMode: () => StockLinkMode;
  isRunning: () => boolean;
}

const validCode = (code: string): boolean => /^\d{6}$/.test(code);

const direction = (mode: StockLinkMode): [LinkerTarget, LinkerTarget] | undefined => {
  if (mode === "ths-to-tdx") return ["ths", "tdx"];
  if (mode === "tdx-to-ths") return ["tdx", "ths"];
  return undefined;
};

export function createStockLinkRuntime(options: StockLinkRuntimeOptions): StockLinkRuntime {
  let mode = options.mode ?? "off";
  let running = false;
  let timer: ReturnType<typeof setTimeout> | undefined;
  let inFlight: Promise<void> | undefined;
  let generation = 0;
  let sourceReady = false;
  let sourceCode = "";
  const interval = Math.max(1, options.pollIntervalMs ?? 300);

  const resetBaseline = (): void => {
    sourceReady = false;
    sourceCode = "";
  };

  const pollOnce = async (): Promise<void> => {
    if (mode === "off") return;
    const state = await options.client.getState();
    options.onState?.(state);
    const targets = direction(mode);
    if (!targets) return;

    const [sourceName, destinationName] = targets;
    const source = state[sourceName];
    const destination = state[destinationName];
    if (!source.available || !validCode(source.code)) {
      resetBaseline();
      return;
    }
    if (!sourceReady) {
      sourceReady = true;
      sourceCode = source.code;
      return;
    }
    if (source.code === sourceCode) return;

    sourceCode = source.code;
    if (destination.available && destination.code === source.code) return;
    const result = await options.client.setCode(destinationName, source.code);
    options.onCommand?.(result);
  };

  const pollNow = (): Promise<void> => {
    if (inFlight) return inFlight;
    const current = pollOnce()
      .catch((error) => {
        options.onError?.(error);
        throw error;
      })
      .finally(() => {
        if (inFlight === current) inFlight = undefined;
      });
    inFlight = current;
    return current;
  };

  const schedule = (expectedGeneration: number): void => {
    if (!running || generation !== expectedGeneration) return;
    timer = globalThis.setTimeout(() => {
      void pollNow().catch(() => undefined).finally(() => schedule(expectedGeneration));
    }, interval);
  };

  const stop = (): void => {
    running = false;
    generation += 1;
    if (timer !== undefined) globalThis.clearTimeout(timer);
    timer = undefined;
    resetBaseline();
  };

  const start = (): (() => void) => {
    if (!running) {
      running = true;
      const expectedGeneration = ++generation;
      void pollNow().catch(() => undefined).finally(() => schedule(expectedGeneration));
    }
    return stop;
  };

  return {
    start,
    stop,
    pollNow,
    setMode: (nextMode) => {
      if (nextMode !== mode) {
        mode = nextMode;
        resetBaseline();
      }
    },
    getMode: () => mode,
    isRunning: () => running,
  };
}
