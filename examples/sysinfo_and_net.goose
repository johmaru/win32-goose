// Win32 System Services, Storage Info, Process Pipe, and WinHTTP/Winsock2 demo.
// Run with: goose run examples/sysinfo_and_net.goose
import std;
import win32 from "..";

fn main() {
    var si = win32::SystemInfo {};
    win32::system_info(si);
    var ms = win32::MemoryStatus {};
    win32::memory_status(ms);
    var lt = win32::SystemTime {};
    win32::local_time(lt);

    print("=== System Info ===");
    print("Computer:   ", win32::computer_name());
    print("User:       ", win32::user_name());
    print("Executable: ", win32::module_file_name());
    print("CPUs:       ", si.processor_count, " (page=", si.page_size, " B)");
    print("RAM:        ", ms.avail_phys / (1024 * 1024), " / ", ms.total_phys / (1024 * 1024), " MiB (", ms.memory_load, "% used)");
    print("Local time: ", lt.year, "-", lt.month, "-", lt.day, " ", lt.hour, ":", lt.minute, ":", lt.second);

    var ds = win32::DiskSpace {};
    var label: u8[>..] = [];
    var fs: u8[>..] = [];
    var serial: u32 = 0;
    if win32::disk_free_space("C:\\", ds) && win32::volume_info("C:\\", label, fs, serial) {
        print("Drive C:\\:  fs=", fs, " free=", ds.free_bytes / (1024 * 1024 * 1024), " / ", ds.total_bytes / (1024 * 1024 * 1024), " GiB");
    }

    print("\n=== Process Pipe Capture ===");
    var out: u8[>..] = [];
    var code: u32 = 0;
    if win32::run_process_capture("cmd.exe /c ver", out, code, 5000) {
        print("exit=", code, " output=", out);
    }

    print("=== Winsock2 DNS ===");
    var ip: u8[>..] = [];
    if win32::dns_resolve_ipv4("localhost", ip) {
        print("localhost -> ", ip);
    }
}
