package main.java.gateway;

import io.grpc.ManagedChannel;
import io.grpc.ManagedChannelBuilder;
import mini_borg.CoordinatorGrpc;
import mini_borg.CoordinatorGrpc.CoordinatorBlockingStub;
import mini_borg.Scheduler.Job;
import mini_borg.Scheduler.SubmitJobRequest;
import mini_borg.Scheduler.SubmitJobResponse;
import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.context.annotation.Bean;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RestController;

@SpringBootApplication
@RestController
public class GatewayApplication {
    // client will wait for a response when using blocking stub
    @Bean
    public static CoordinatorBlockingStub coordinatorClient() {
        ManagedChannel channel = ManagedChannelBuilder.forAddress("localhost", 50051).usePlaintext().build();
        return CoordinatorGrpc.newBlockingStub(channel);
    }

    private final CoordinatorBlockingStub stub;

    // Spring auto passes Bean created above into this constructor
    public GatewayApplication(CoordinatorBlockingStub stub) {
        this.stub = stub;
    }

    public static void main(String[] args) {
        SpringApplication.run(GatewayApplication.class, args); // boots up web server
    }

    @PostMapping("/submit")
    public String submitJob(@RequestBody JobSubmissionDto dto) {
        // Resource message is nested so needs to be built first then plugged in
        mini_borg.Scheduler.Resource resourceProto = mini_borg.Scheduler.Resource.newBuilder()
                                                         .setCpuCores(dto.getCpu_cores())
                                                         .setMemoryMb(dto.getMemory_mb())
                                                         .build();

        // the actual reqest with resourceProto plugged in
        mini_borg.Scheduler.SubmitJobRequest request = mini_borg.Scheduler.SubmitJobRequest.newBuilder()
                                                           .setName(dto.getName())
                                                           .setResourceReqs(resourceProto)
                                                           .build();

        mini_borg.Scheduler.SubmitJobResponse response = stub.submitJob(request);

        return response.getJobId();
    }
}