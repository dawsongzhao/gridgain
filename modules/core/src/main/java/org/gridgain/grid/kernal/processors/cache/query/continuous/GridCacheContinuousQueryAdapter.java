/* @java.file.header */

/*  _________        _____ __________________        _____
 *  __  ____/___________(_)______  /__  ____/______ ____(_)_______
 *  _  / __  __  ___/__  / _  __  / _  / __  _  __ `/__  / __  __ \
 *  / /_/ /  _  /    _  /  / /_/ /  / /_/ /  / /_/ / _  /  _  / / /
 *  \____/   /_/     /_/   \_,__/   \____/   \__,_/  /_/   /_/ /_/
 */

package org.gridgain.grid.kernal.processors.cache.query.continuous;

import org.gridgain.grid.*;
import org.gridgain.grid.cache.*;
import org.gridgain.grid.cache.query.*;
import org.gridgain.grid.cache.query.GridCacheContinuousQueryEntry;
import org.gridgain.grid.kernal.processors.cache.*;
import org.gridgain.grid.kernal.processors.continuous.*;
import org.gridgain.grid.lang.*;
import org.gridgain.grid.logger.*;
import org.gridgain.grid.security.*;
import org.gridgain.grid.util.*;
import org.gridgain.grid.util.typedef.*;
import org.gridgain.grid.util.typedef.internal.*;
import org.jetbrains.annotations.*;

import java.util.*;
import java.util.concurrent.locks.*;

import static org.gridgain.grid.cache.GridCacheDistributionMode.*;

/**
 * Continuous query implementation.
 */
public class GridCacheContinuousQueryAdapter<K, V> implements GridCacheContinuousQuery<K, V> {
    /** Guard. */
    private final GridBusyLock guard = new GridBusyLock();

    /** Close lock. */
    private final Lock closeLock = new ReentrantLock();

    /** Cache context. */
    private final GridCacheContext<K, V> ctx;

    /** Topic for ordered messages. */
    private final Object topic;

    /** Projection predicate */
    private final GridPredicate<GridCacheEntry<K, V>> prjPred;

    /** Keep portable flag. */
    private final boolean keepPortable;

    /** Logger. */
    private final GridLogger log;

    /** Local callback. */
    private volatile GridBiPredicate<UUID, Collection<Map.Entry<K, V>>> cb;

    /** Local callback. */
    private volatile GridBiPredicate<UUID, Collection<GridCacheContinuousQueryEntry<K, V>>> locCb;

    /** Filter. */
    private volatile GridBiPredicate<K, V> filter;

    /** Remote filter. */
    private volatile GridPredicate<GridCacheContinuousQueryEntry<K, V>> rmtFilter;

    /** Buffer size. */
    private volatile int bufSize = DFLT_BUF_SIZE;

    /** Time interval. */
    @SuppressWarnings("RedundantFieldInitialization")
    private volatile long timeInterval = DFLT_TIME_INTERVAL;

    /** Automatic unsubscribe flag. */
    private volatile boolean autoUnsubscribe = DFLT_AUTO_UNSUBSCRIBE;

    /** Continuous routine ID. */
    private UUID routineId;

    /**
     * @param ctx Cache context.
     * @param topic Topic for ordered messages.
     * @param prjPred Projection predicate.
     */
    GridCacheContinuousQueryAdapter(GridCacheContext<K, V> ctx, Object topic,
        @Nullable GridPredicate<GridCacheEntry<K, V>> prjPred) {
        assert ctx != null;
        assert topic != null;

        this.ctx = ctx;
        this.topic = topic;
        this.prjPred = prjPred;

        keepPortable = ctx.keepPortable();

        log = ctx.logger(getClass());
    }

    /** {@inheritDoc} */
    @Override public void callback(final GridBiPredicate<UUID, Collection<Map.Entry<K, V>>> cb) {
        if (cb != null) {
            this.cb = cb;

            localCallback(new CallbackWrapper<>(cb));
        }
        else
            localCallback(null);
    }

    /** {@inheritDoc} */
    @Nullable @Override public GridBiPredicate<UUID, Collection<Map.Entry<K, V>>> callback() {
        return cb;
    }

    /** {@inheritDoc} */
    @Override public void filter(final GridBiPredicate<K, V> filter) {
        if (filter != null) {
            this.filter = filter;

            remoteFilter(new FilterWrapper<>(filter));
        }
        else
            remoteFilter(null);
    }

    /** {@inheritDoc} */
    @Nullable @Override public GridBiPredicate<K, V> filter() {
        return filter;
    }

    /** {@inheritDoc} */
    @Override public void localCallback(GridBiPredicate<UUID, Collection<GridCacheContinuousQueryEntry<K, V>>> locCb) {
        if (!guard.enterBusy())
            throw new IllegalStateException("Continuous query can't be changed after it was executed.");

        try {
            this.locCb = locCb;
        }
        finally {
            guard.leaveBusy();
        }
    }

    /** {@inheritDoc} */
    @Nullable @Override public GridBiPredicate<UUID, Collection<GridCacheContinuousQueryEntry<K, V>>> localCallback() {
        return locCb;
    }

    /** {@inheritDoc} */
    @Override public void remoteFilter(@Nullable GridPredicate<GridCacheContinuousQueryEntry<K, V>> rmtFilter) {
        if (!guard.enterBusy())
            throw new IllegalStateException("Continuous query can't be changed after it was executed.");

        try {
            this.rmtFilter = rmtFilter;
        }
        finally {
            guard.leaveBusy();
        }
    }

    /** {@inheritDoc} */
    @Nullable @Override public GridPredicate<GridCacheContinuousQueryEntry<K, V>> remoteFilter() {
        return rmtFilter;
    }

    /** {@inheritDoc} */
    @Override public void bufferSize(int bufSize) {
        A.ensure(bufSize > 0, "bufSize > 0");

        if (!guard.enterBusy())
            throw new IllegalStateException("Continuous query can't be changed after it was executed.");

        try {
            this.bufSize = bufSize;
        }
        finally {
            guard.leaveBusy();
        }
    }

    /** {@inheritDoc} */
    @Override public int bufferSize() {
        return bufSize;
    }

    /** {@inheritDoc} */
    @Override public void timeInterval(long timeInterval) {
        A.ensure(timeInterval >= 0, "timeInterval >= 0");

        if (!guard.enterBusy())
            throw new IllegalStateException("Continuous query can't be changed after it was executed.");

        try {
            this.timeInterval = timeInterval;
        }
        finally {
            guard.leaveBusy();
        }
    }

    /** {@inheritDoc} */
    @Override public long timeInterval() {
        return timeInterval;
    }

    /** {@inheritDoc} */
    @Override public void autoUnsubscribe(boolean autoUnsubscribe) {
        this.autoUnsubscribe = autoUnsubscribe;
    }

    /** {@inheritDoc} */
    @Override public boolean isAutoUnsubscribe() {
        return autoUnsubscribe;
    }

    /** {@inheritDoc} */
    @Override public void execute() throws GridException {
        execute(null, false);
    }

    /** {@inheritDoc} */
    @Override public void execute(@Nullable GridProjection prj) throws GridException {
        execute(prj, false);
    }

    /**
     * Starts continuous query execution.
     *
     * @param prj Grid projection.
     * @param internal If {@code true} then query notified about internal entries updates.
     * @throws GridException If failed.
     */
    public void execute(@Nullable GridProjection prj, boolean internal) throws GridException {
        if (locCb == null)
            throw new IllegalStateException("Mandatory local callback is not set for the query: " + this);

        ctx.checkSecurity(GridSecurityPermission.CACHE_READ);

        if (prj == null)
            prj = ctx.grid();

        prj = prj.forCache(ctx.name());

        if (prj.nodes().isEmpty())
            throw new GridTopologyException("Failed to execute continuous query (projection is empty): " + this);

        boolean skipPrimaryCheck = false;

        Collection<GridNode> nodes = prj.nodes();

        if (nodes.isEmpty())
            throw new GridTopologyException("Failed to execute continuous query (empty projection is provided): " +
                this);

        switch (ctx.config().getCacheMode()) {
            case LOCAL:
                if (!nodes.contains(ctx.localNode()))
                    throw new GridTopologyException("Continuous query for LOCAL cache can be executed only locally (" +
                        "provided projection contains remote nodes only): " + this);
                else if (nodes.size() > 1)
                    U.warn(log, "Continuous query for LOCAL cache will be executed locally (provided projection is " +
                        "ignored): " + this);

                prj = prj.forNode(ctx.localNode());

                break;

            case REPLICATED:
                if (nodes.size() == 1 && F.first(nodes).equals(ctx.localNode())) {
                    GridCacheDistributionMode distributionMode = ctx.config().getDistributionMode();

                    if (distributionMode == PARTITIONED_ONLY || distributionMode == NEAR_PARTITIONED)
                        skipPrimaryCheck = true;
                }

                break;
        }

        closeLock.lock();

        try {
            if (routineId != null)
                throw new IllegalStateException("Continuous query can't be executed twice.");

            guard.block();

            GridContinuousHandler hnd = ctx.kernalContext().security().enabled() ? keepPortable ?
                new GridCacheContinuousQueryHandlerV4<>(ctx.name(), topic, locCb, rmtFilter, prjPred, internal,
                    skipPrimaryCheck, ctx.kernalContext().job().currentTaskNameHash()) :
                new GridCacheContinuousQueryHandlerV2<>(ctx.name(), topic, locCb, rmtFilter, prjPred, internal,
                    skipPrimaryCheck, ctx.kernalContext().job().currentTaskNameHash()) : keepPortable ?
                new GridCacheContinuousQueryHandlerV3<>(ctx.name(), topic, locCb, rmtFilter, prjPred, internal,
                    skipPrimaryCheck) :
                new GridCacheContinuousQueryHandler<>(ctx.name(), topic, locCb, rmtFilter, prjPred, internal,
                    skipPrimaryCheck);

            routineId = ctx.kernalContext().continuous().startRoutine(hnd, bufSize, timeInterval, autoUnsubscribe,
                prj.predicate()).get();
        }
        finally {
            closeLock.unlock();
        }
    }

    /** {@inheritDoc} */
    @Override public void close() throws GridException {
        closeLock.lock();

        try {
            if (routineId != null)
                ctx.kernalContext().continuous().stopRoutine(routineId).get();
        }
        finally {
            closeLock.unlock();
        }
    }

    /** {@inheritDoc} */
    @Override public String toString() {
        return S.toString(GridCacheContinuousQueryAdapter.class, this);
    }

    /**
     * Deprecated callback wrapper.
     */
    static class CallbackWrapper<K, V> implements GridBiPredicate<UUID, Collection<GridCacheContinuousQueryEntry<K, V>>> {
        /** Serialization ID. */
        private static final long serialVersionUID = 0L;

        /** */
        private final GridBiPredicate<UUID, Collection<Map.Entry<K, V>>> cb;

        /**
         * @param cb Deprecated callback.
         */
        private CallbackWrapper(GridBiPredicate<UUID, Collection<Map.Entry<K, V>>> cb) {
            this.cb = cb;
        }

        /** {@inheritDoc} */
        @SuppressWarnings("unchecked")
        @Override public boolean apply(UUID nodeId, Collection<GridCacheContinuousQueryEntry<K, V>> entries) {
            return cb.apply(nodeId, (Collection<Map.Entry<K,V>>)(Collection)entries);
        }
    }

    /**
     * Deprecated filter wrapper.
     */
    static class FilterWrapper<K, V> implements GridPredicate<GridCacheContinuousQueryEntry<K, V>> {
        /** Serialization ID. */
        private static final long serialVersionUID = 0L;

        /** */
        private final GridBiPredicate<K, V> filter;

        /**
         * @param filter Deprecated callback.
         */
        FilterWrapper(GridBiPredicate<K, V> filter) {
            this.filter = filter;
        }

        /** {@inheritDoc} */
        @SuppressWarnings("unchecked")
        @Override public boolean apply(GridCacheContinuousQueryEntry<K, V> entry) {
            return filter.apply(entry.getKey(), entry.getValue());
        }
    }
}
