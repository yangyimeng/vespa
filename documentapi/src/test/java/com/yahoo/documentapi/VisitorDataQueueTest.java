// Copyright 2017 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.documentapi;

import com.yahoo.document.Document;
import com.yahoo.document.DocumentId;
import com.yahoo.document.DocumentPut;
import com.yahoo.document.DocumentRemove;
import com.yahoo.document.DocumentTypeManager;
import com.yahoo.document.DocumentTypeManagerConfigurer;
import com.yahoo.documentapi.messagebus.protocol.GetDocumentMessage;
import com.yahoo.documentapi.messagebus.protocol.PutDocumentMessage;
import com.yahoo.documentapi.messagebus.protocol.RemoveDocumentMessage;
import com.yahoo.vdslib.Entry;
import org.junit.Before;
import org.junit.Test;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.core.IsNull.notNullValue;
import static org.hamcrest.core.IsNull.nullValue;
import static org.hamcrest.core.IsInstanceOf.instanceOf;
import static org.hamcrest.core.Is.is;
import static org.junit.Assert.assertThat;

public class VisitorDataQueueTest {

    private final DocumentTypeManager docMan = new DocumentTypeManager();

    @Before
    public void setUp() {
        DocumentTypeManagerConfigurer.configure(docMan, "file:./test/cfg/testdoc.cfg");
    }

    private PutDocumentMessage createPutMessage(final String docId) {
        return new PutDocumentMessage(new DocumentPut(new Document(docMan.getDocumentType("testdoc"), docId)));
    }

    private static RemoveDocumentMessage createRemoveMessage(final String docId) {
        return new RemoveDocumentMessage(new DocumentId(docId));
    }

    private static AckToken createDummyAckToken() {
        return new AckToken(new Object());
    }

    private static void assertNonNullDocumentListResponse(final VisitorResponse response) {
        assertThat(response, notNullValue());
        assertThat(response, instanceOf(DocumentListVisitorResponse.class));
    }

    private static void assertResponseHasSinglePut(final VisitorResponse response, final DocumentPut expectedInstance) {
        assertNonNullDocumentListResponse(response);
        final DocumentListVisitorResponse visitorResponse = (DocumentListVisitorResponse)response;
        assertThat(visitorResponse.getDocumentList().size(), equalTo(1));
        final Entry entry = visitorResponse.getDocumentList().get(0);
        assertThat(entry.getDocumentOperation(), is(expectedInstance));
    }

    @Test
    public void received_put_can_be_polled_via_non_timeout_getter() {
        final VisitorDataQueue queue = new VisitorDataQueue();
        final PutDocumentMessage putMessage = createPutMessage("id:foo:testdoc::foo");
        queue.onMessage(putMessage, createDummyAckToken());
        final VisitorResponse response = queue.getNext();

        assertResponseHasSinglePut(response, putMessage.getDocumentPut());
        assertThat(queue.getNext(), nullValue()); // Queue now empty
    }

    @Test
    public void received_put_can_be_polled_via_timeout_getter() throws InterruptedException {
        final VisitorDataQueue queue = new VisitorDataQueue();
        final PutDocumentMessage putMessage = createPutMessage("id:foo:testdoc::foo");
        queue.onMessage(putMessage, createDummyAckToken());
        final VisitorResponse response = queue.getNext(1000);

        assertResponseHasSinglePut(response, putMessage.getDocumentPut());
        assertThat(queue.getNext(), nullValue()); // Queue now empty
    }

    private static void assertResponseHasSingleRemove(final VisitorResponse response, final String docId) {
        assertNonNullDocumentListResponse(response);
        final DocumentListVisitorResponse visitorResponse = (DocumentListVisitorResponse)response;
        assertThat(visitorResponse.getDocumentList().size(), equalTo(1));
        final Entry entry = visitorResponse.getDocumentList().get(0);
        assertThat(entry.isRemoveEntry(), is(true));
        assertThat(entry.getDocumentOperation(), instanceOf(DocumentRemove.class));
        assertThat(entry.getDocumentOperation().getId(), equalTo(new DocumentId(docId)));
    }

    @Test
    public void received_remove_can_be_polled_via_non_timeout_getter() {
        final VisitorDataQueue queue = new VisitorDataQueue();
        queue.onMessage(createRemoveMessage("id:foo:testdoc::bar"), createDummyAckToken());
        final VisitorResponse response = queue.getNext();

        assertResponseHasSingleRemove(response, "id:foo:testdoc::bar");
    }

    @Test
    public void received_remove_can_be_polled_via_non_getter() throws InterruptedException {
        final VisitorDataQueue queue = new VisitorDataQueue();
        queue.onMessage(createRemoveMessage("id:foo:testdoc::bar"), createDummyAckToken());
        final VisitorResponse response = queue.getNext(1000);

        assertResponseHasSingleRemove(response, "id:foo:testdoc::bar");
    }

    @Test
    public void multiple_messages_are_enqueued_and_dequeued_in_fifo_order() {
        final VisitorDataQueue queue = new VisitorDataQueue();
        final PutDocumentMessage firstPut = createPutMessage("id:foo:testdoc::foo");
        final PutDocumentMessage secondPut = createPutMessage("id:foo:testdoc::baz");

        queue.onMessage(firstPut, createDummyAckToken());
        queue.onMessage(createRemoveMessage("id:foo:testdoc::bar"), createDummyAckToken());
        queue.onMessage(secondPut, createDummyAckToken());
        queue.onMessage(createRemoveMessage("id:foo:testdoc::fleeb"), createDummyAckToken());

        assertResponseHasSinglePut(queue.getNext(), firstPut.getDocumentPut());
        assertResponseHasSingleRemove(queue.getNext(), "id:foo:testdoc::bar");
        assertResponseHasSinglePut(queue.getNext(), secondPut.getDocumentPut());
        assertResponseHasSingleRemove(queue.getNext(), "id:foo:testdoc::fleeb");
    }

    @Test(expected = UnsupportedOperationException.class)
    public void unknown_message_throws_unsupported_operation_exception() {
        final VisitorDataQueue queue = new VisitorDataQueue();
        queue.onMessage(new GetDocumentMessage(new DocumentId("id:foo:testdoc::bar")), createDummyAckToken());
    }

}
