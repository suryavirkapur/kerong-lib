declare class BoardState {
    boardAddress: number;
    locks_1_8: number;
    locks_9_16: number;
    ir_1_8: number;
    ir_9_16: number;
    isLocked(index: number): boolean;
    isUnlocked(index: number): boolean;
    isItemDetected(index: number): boolean;
}

declare class KerongClient {
    constructor();
    connect(ip: string, port: number, timeoutMs?: number): Promise<void>;
    disconnect(): void;
    unlock(board: number, lock: number): Promise<void>;
    getState(board: number): Promise<BoardState>;
    isConnected(): boolean;
}

export { BoardState, KerongClient };
export function boardStateToObject(state: BoardState): BoardState;
