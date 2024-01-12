/* Implement this class. */

import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;

public class MyDispatcher extends Dispatcher {
    private final AtomicInteger roundRobinIndex = new AtomicInteger(0);

    public MyDispatcher(SchedulingAlgorithm algorithm, List<Host> hosts) {
        super(algorithm, hosts);
    }

    @Override
    public void addTask(Task task) {
        switch (this.algorithm) {
            case ROUND_ROBIN -> {
                int currentIndex = roundRobinIndex.getAndIncrement();
                hosts.get(currentIndex % hosts.size()).addTask(task);
            }
            case SHORTEST_QUEUE ->  hosts.stream().min((h1, h2) -> {
                int result = Integer.compare(h1.getQueueSize(), h2.getQueueSize());

                if (result == 0) {
                    return Integer.compare(hosts.indexOf(h1), hosts.indexOf(h2) );
                }
                return result;
            }).ifPresent(host -> host.addTask(task));
            case LEAST_WORK_LEFT -> hosts.stream().min((h1, h2) -> {
                long result = h1.getWorkLeft() - h2.getWorkLeft();

                if (result < MyHost.COMPARE_THRESHOLD * 10) {
                    return Integer.compare(hosts.indexOf(h1), hosts.indexOf(h2));
                }
                return (int) result;
            }).ifPresent(host -> host.addTask(task));

            case SIZE_INTERVAL_TASK_ASSIGNMENT -> {
                switch (task.getType()) {
                    case SHORT -> hosts.get(TaskType.SHORT.ordinal()).addTask(task);
                    case MEDIUM -> hosts.get(TaskType.MEDIUM.ordinal()).addTask(task);
                    case LONG -> hosts.get(TaskType.LONG.ordinal()).addTask(task);
                }
            }
        }
    }
}
