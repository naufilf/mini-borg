package main.java.gateway;
public class JobSubmissionDto {
    private String name;
    private int cpu_cores;
    private long memory_mb;

    public JobSubmissionDto() {}

    public JobSubmissionDto(String name, int cpu_cores, long memory_mb) {
        this.name = name;
        this.cpu_cores = cpu_cores;
        this.memory_mb = memory_mb;
    }

    public String getName() {
        return name;
    }

    public void setName(String name) {
        this.name = name;
    }

    public int getCpu_cores() {
        return cpu_cores;
    }

    public void setCpu_cores(int cpu_cores) {
        this.cpu_cores = cpu_cores;
    }

    public long getMemory_mb() {
        return memory_mb;
    }

    public void setMemory_mb(long memory_mb) {
        this.memory_mb = memory_mb;
    }
}