-- ═══════════════════════════════════════════════════════════
-- Supabase Setup for Huonyx Watch Flipper Bridge
-- 
-- This creates optional tables for command logging and device
-- registry. The Realtime Broadcast channel works without these,
-- but they provide persistence and history.
-- ═══════════════════════════════════════════════════════════

-- ── Device Registry ────────────────────────────────────────
-- Tracks connected watches and their Flipper Zero devices
CREATE TABLE IF NOT EXISTS watch_devices (
    id UUID DEFAULT gen_random_uuid() PRIMARY KEY,
    device_id TEXT NOT NULL UNIQUE,
    device_name TEXT DEFAULT 'Huonyx Watch',
    flipper_name TEXT,
    firmware_version TEXT,
    last_seen TIMESTAMPTZ DEFAULT NOW(),
    battery_level INTEGER DEFAULT 0,
    is_online BOOLEAN DEFAULT FALSE,
    flipper_connected BOOLEAN DEFAULT FALSE,
    metadata JSONB DEFAULT '{}',
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW()
);

-- ── Command Log ────────────────────────────────────────────
-- Logs all Flipper commands sent through the bridge
CREATE TABLE IF NOT EXISTS flipper_commands (
    id BIGSERIAL PRIMARY KEY,
    device_id TEXT NOT NULL,
    command TEXT NOT NULL,
    source TEXT DEFAULT 'agent',
    cmd_id INTEGER,
    status TEXT DEFAULT 'pending',  -- pending, sent, success, error, timeout
    result TEXT,
    sent_at TIMESTAMPTZ DEFAULT NOW(),
    completed_at TIMESTAMPTZ,
    duration_ms INTEGER
);

-- ── Indexes ────────────────────────────────────────────────
CREATE INDEX IF NOT EXISTS idx_flipper_commands_device 
    ON flipper_commands(device_id);
CREATE INDEX IF NOT EXISTS idx_flipper_commands_status 
    ON flipper_commands(status);
CREATE INDEX IF NOT EXISTS idx_flipper_commands_sent 
    ON flipper_commands(sent_at DESC);
CREATE INDEX IF NOT EXISTS idx_watch_devices_online 
    ON watch_devices(is_online);

-- ── Auto-update timestamps ─────────────────────────────────
CREATE OR REPLACE FUNCTION update_updated_at()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER watch_devices_updated
    BEFORE UPDATE ON watch_devices
    FOR EACH ROW EXECUTE FUNCTION update_updated_at();

-- ── RLS Policies ───────────────────────────────────────────
-- Enable RLS (adjust based on your auth setup)
ALTER TABLE watch_devices ENABLE ROW LEVEL SECURITY;
ALTER TABLE flipper_commands ENABLE ROW LEVEL SECURITY;

-- Allow authenticated users to read/write (adjust as needed)
CREATE POLICY "Allow all for authenticated" ON watch_devices
    FOR ALL USING (true) WITH CHECK (true);

CREATE POLICY "Allow all for authenticated" ON flipper_commands
    FOR ALL USING (true) WITH CHECK (true);

-- ═══════════════════════════════════════════════════════════
-- NOTES:
-- 
-- 1. The Realtime Broadcast channel "flipper-bridge" works
--    without these tables. They are for optional logging.
--
-- 2. Adjust RLS policies based on your authentication setup.
--    The above policies are permissive for development.
--
-- 3. The watch firmware does NOT write to these tables directly.
--    The agent-side script (flipper_bridge.py) can optionally
--    log commands here.
-- ═══════════════════════════════════════════════════════════
