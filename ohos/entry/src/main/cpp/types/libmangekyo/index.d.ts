// Functions of libmangekyo.so, implemented in ../../napi_init.cpp.

/** Tells the engine where the SPIR-V shaders are and where results go. */
export const initPaths: (shaderDir: string, dataDir: string) => void;

export const engineVersion: () => string;

/** True once the preview surface of the GPU page exists. */
export const hasSurface: () => boolean;

/** Starts "stream" or "gpu_burn" in the preview surface. False when it could not start; see lastError(). */
export const startWorkload: (workloadId: string, seconds: number) => boolean;

export const stopWorkload: () => void;

/** True while a GPU test runs. */
export const isRunning: () => boolean;

export const lastError: () => string;

/** Lines the engine printed since the last call. */
export const pollOutput: () => string[];

/** Runs the engine's command line with these arguments on a native thread. Used for the CPU tests. */
export const startCli: (args: string[]) => boolean;

export const isCliRunning: () => boolean;

export const cliExitCode: () => number;
