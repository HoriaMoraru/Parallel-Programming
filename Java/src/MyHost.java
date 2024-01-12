/* Implement this class. */
import java.util.Comparator;
import java.util.Optional;
import java.util.PriorityQueue;
import java.util.Queue;

public class MyHost extends Host {
    public static final long COMPARE_THRESHOLD = 100L;
    private final Queue<Task> waiting;
    private Optional<Task> running;
    private boolean shutdown;

    public MyHost() {
        this.waiting = new PriorityQueue<>(
                Comparator.comparingInt(Task::getPriority).reversed()
                        .thenComparingLong(Task::getStart)
        );
        this.running = Optional.empty();
        this.shutdown = false;
    }

    @Override
    public void run() {
        while (!shutdown) {
            if (running.isEmpty()) {
                running = Optional.ofNullable(pollTask());
            }
            if (running.isPresent()) {
                if (shouldPreempt()) {
                    preemptTask(running.get());
                }
                executeTask(running.get());
            }
        }
    }

    private void preemptTask(Task task) {
        running = Optional.ofNullable(pollTask());
        addTask(task);
    }

    private void executeTask(Task task) {
        try {
            Thread.sleep(COMPARE_THRESHOLD);
            task.setLeft(task.getLeft() - COMPARE_THRESHOLD);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        } finally {
            if (task.getLeft() <= 0) {
                task.finish();
                running = Optional.empty();
            }
        }
    }

    private boolean shouldPreempt() {
        assert running.isPresent();
        return !waiting.isEmpty() && running.get().isPreemptible() && waiting.peek().getPriority() > running.get().getPriority();
    }

    private synchronized Task pollTask() {
        return waiting.poll();
    }

    @Override
    public synchronized void addTask(Task task) {
        waiting.add(task);
    }
    @Override
    public int getQueueSize() {
        return running.isPresent() ? waiting.size() + 1 : waiting.size();
    }

    @Override
    public long getWorkLeft() {
        return waiting.stream().mapToLong(Task::getLeft).sum() + (running.isPresent() ? running.get().getLeft() : 0L);
    }

    @Override
    public void shutdown() {
        shutdown = true;
    }
}