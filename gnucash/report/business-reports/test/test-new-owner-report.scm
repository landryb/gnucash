(use-modules (gnucash gnc-module))
(gnc:module-begin-syntax (gnc:module-load "gnucash/app-utils" 0))
(use-modules (gnucash engine test test-extras))
(use-modules (gnucash report invoice))
(use-modules (gnucash report stylesheets))
(use-modules (gnucash report report-system))
(use-modules (gnucash report report-system test test-extras))
(use-modules (srfi srfi-1))
(use-modules (srfi srfi-64))
(use-modules (gnucash engine test srfi64-extras))
(use-modules (sxml simple))
(use-modules (sxml xpath))
(use-modules (system vm coverage))
(use-modules (system vm vm))

(define uuid "c146317be32e4948a561ec7fc89d15c1-new")

(setlocale LC_ALL "C")

(define (run-test)
  (if #f
      (coverage-test run-test-proper)
      (run-test-proper)))

(define (coverage-test tester)
  (let* ((currfile (dirname (current-filename)))
         (path (string-take currfile (string-rindex currfile #\/))))
    (add-to-load-path path))
  (call-with-values
      (lambda()
        (with-code-coverage tester))
    (lambda (data result)
      (let ((port (open-output-file "/tmp/lcov.info")))
        (coverage-data->lcov data port)
        (close port)))))

(define (run-test-proper)
  (let ((saved-format (qof-date-format-get)))
    (qof-date-format-set QOF-DATE-FORMAT-ISO)
    (test-runner-factory gnc:test-runner)
    (test-begin "test-new-owner-report")
    (owner-tests)
    (qof-date-format-set saved-format)
    (test-end "test-new-owner-report")))

(define (sxml-get-row-col sxml row col)
  (sxml->table-row-col sxml 3 row col))

(define (set-option! options section name value)
  (let ((option (gnc:lookup-option options section name)))
    (if option
        (gnc:option-set-value option value)
        (test-assert (format #f "wrong-option ~a ~a" section name) #f))))

(define (get-currency sym)
  (gnc-commodity-table-lookup
   (gnc-commodity-table-get-table (gnc-get-current-book))
   (gnc-commodity-get-namespace (gnc-default-report-currency))
   sym))

(define structure
  (list "Root" (list (cons 'type ACCT-TYPE-ASSET)
                     (cons 'commodity (get-currency "USD")))
        (list "Asset"
              (list "Bank-USD"))
        (list "VAT"
              (list "VAT-on-Purchases")
              (list "VAT-on-Sales" (list (cons 'type ACCT-TYPE-LIABILITY))))
        (list "Income" (list (cons 'type ACCT-TYPE-INCOME))
              (list "Income-USD"))
        (list "A/Receivable" (list (cons 'type ACCT-TYPE-RECEIVABLE))
              (list "AR-USD"))
        (list "A/Payable" (list (cons 'type ACCT-TYPE-PAYABLE))
              (list "AP-USD"))))

(define (options->sxml options test-title)
  (gnc:options->sxml uuid options "test-new-owner-report" test-title))

(define (owner-tests)
  (let* ((env (create-test-env))
         (account-alist (env-create-account-structure-alist env structure))
         (get-acct (lambda (name)
                     (or (assoc-ref account-alist name)
                         (error "invalid account name" name))))
         (AR (get-acct "AR-USD"))
         (Bank (get-acct "Bank-USD"))
         (YEAR (gnc:time64-get-year (gnc:get-today))))

    (define owner-1
      (let* ((cust-1 (gncCustomerCreate (gnc-get-current-book)))
             (owner-1 (gncOwnerNew)))
        (gncCustomerSetID cust-1 "cust-1-id")
        (gncCustomerSetName cust-1 "cust-1-name")
        (gncCustomerSetNotes cust-1 "cust-1-notes")
        (gncCustomerSetCurrency cust-1 (get-currency "USD"))
        (gncCustomerSetTaxIncluded cust-1 1)
        (gncOwnerInitCustomer owner-1 cust-1)
        owner-1))

    ;; inv-1 is generated for a customer
    (define inv-1
      (let ((inv-1 (gncInvoiceCreate (gnc-get-current-book))))
        (gncInvoiceSetOwner inv-1 owner-1)
        (gncInvoiceSetNotes inv-1 "inv-1-notes")
        (gncInvoiceSetBillingID inv-1 "inv-1-billing-id")
        (gncInvoiceSetCurrency inv-1 (get-currency "USD"))
        inv-1))

    (define (make-entry amt)
      (let ((entry (gncEntryCreate (gnc-get-current-book))))
        (gncEntrySetDateGDate entry (time64-to-gdate (current-time)))
        (gncEntrySetDescription entry "entry-desc")
        (gncEntrySetAction entry "entry-action")
        (gncEntrySetNotes entry "entry-notes")
        (gncEntrySetInvAccount entry (get-acct "Income-USD"))
        (gncEntrySetDocQuantity entry 1 #f)
        (gncEntrySetInvPrice entry amt)
        entry))

    (define (add-invoice date amount desc)
      (let ((inv (gncInvoiceCopy inv-1)))
        (gncInvoiceAddEntry inv (make-entry amount))
        (gncInvoicePostToAccount inv
                                 (get-acct "AR-USD")         ;post-to acc
                                 date                        ;posted-date
                                 date                        ;due-date
                                 desc                        ;desc
                                 #t                          ;acc-splits?
                                 #f)                         ;autopay
        inv))

    (define (create-split-for-lot account amount lot)
      (let ((split (xaccMallocSplit (gnc-get-current-book))))
        (xaccSplitSetAccount split account)
        (xaccSplitSetAmount split amount)
        (xaccSplitSetValue split amount)
        (xaccSplitSetAction split "")
        (when lot
          (gnc-lot-set-title lot "")
          (gnc-lot-add-split lot split))
        split))

    (define (create-multisplit DD MM YY desc type splits)
      (let* ((book (gnc-get-current-book))
             (txn (xaccMallocTransaction book)))
        (xaccTransBeginEdit txn)
        (xaccTransSetDescription txn desc)
        (xaccTransSetCurrency txn (xaccAccountGetCommodity
                                   (xaccSplitGetAccount (car splits))))
        (xaccTransSetDate txn DD MM YY)
        (xaccTransSetTxnType txn type)
        (for-each (lambda (s) (xaccSplitSetParent s txn)) splits)
        (xaccTransCommitEdit txn)
        txn))

    (define (default-testing-options owner account)
      ;; owner-report will run from 1.1.1980 to 1.7.1980
      (let ((options (gnc:make-report-options uuid)))
        (set-option! options "General" "Customer" owner)
        (set-option! options "General" "From"
                     (cons 'absolute (gnc-dmy2time64 1 1 1980)))
        (set-option! options "General" "To"
                     (cons 'absolute (gnc-dmy2time64 1 7 1980)))
        (set-option! options "Display Columns" "Links" 'detailed)
        options))

    ;; inv $6.75
    (add-invoice (gnc-dmy2time64 13 05 1980) 27/4 "$6.75")

    ;; inv $11.50, 2 payments
    (let ((inv (add-invoice (gnc-dmy2time64 13 01 1980) 23/2 "$11.50")))
      (gncInvoiceApplyPayment inv '() Bank 3/2 1
                              (gnc-dmy2time64 18 03 1980)
                              "inv >90 payment" "pay only $1.50")

      (gncInvoiceApplyPayment inv '() Bank 2 1
                              (gnc-dmy2time64 20 03 1980)
                              "inv >90 payment" "pay only $2.00"))

    ;; CN $3.00
    (let ((new-cn (add-invoice (gnc-dmy2time64 22 06 1980) -3 "CN")))
      (gncInvoiceSetIsCreditNote new-cn #t))

    ;; inv $11.50, 2 payments
    (let* ((inv (add-invoice (gnc-dmy2time64 24 06 1980) 28 "$28.00"))
           (CN (add-invoice (gnc-dmy2time64 25 06 1980) -27 "$27.00"))
           (inv-lot (gncInvoiceGetPostedLot inv))
           (CN-lot (gncInvoiceGetPostedLot CN)))
      (gncInvoiceSetIsCreditNote CN #t)

      (create-multisplit
       26 06 1980 "payment" TXN-TYPE-PAYMENT
       (list (create-split-for-lot AR -1 inv-lot)
             (create-split-for-lot Bank 1 #f)))

      (create-multisplit
       26 06 1980 "payment" TXN-TYPE-LINK
       (list (create-split-for-lot AR -27 inv-lot)
             (create-split-for-lot AR 27 CN-lot))))

    (display "new-owner-report tests:\n")
    (test-begin "new-customer-report")
    (let* ((options (default-testing-options owner-1 (get-acct "AR-USD")))
           (sxml (options->sxml options "new-customer-report basic")))
      (test-equal "line 1"
        '("Customer History" "Linked Details" "1980-01-13" "1980-01-13"
          "Invoice" "$11.50" "$11.50" "1980-03-18" "Payment" "inv >90 payment"
          "$11.50" "pay only $1.50" "$1.50" "$1.50" "Pre-Payment" "Current"
          "0-30 days" "31-60 days" "61-90 days" "91+ days" "Total" "$0.00"
          "$0.00" "-$3.00" "$6.75" "$0.00" "$8.00" "$11.75")
        ((sxpath `(// (table 3) // (tr 1) // *text*)) sxml))
      (test-equal "line 2"
        '("Date" "Due Date" "Reference" "Type" "Description" "Invoice"
          "Payment" "Balance" "Date" "Reference" "Type" "Description"
          "Partial Amount" "Amount" "1980-03-20" "Payment" "inv >90 payment"
          "pay only $2.00" "$2.00" "$2.00")
        ((sxpath `(// (table 3) // (tr 2) // *text*)) sxml))
      (test-equal "line 3"
        '("UNPAID" "$8.00")
        ((sxpath `(// (table 3) // (tr 3) // *text*)) sxml))
      (test-equal "line 4"
        '("1980-03-18" "Payment" "inv >90 payment" "$10.00" "1980-01-13"
          "Invoice" "inv >90 payment" "pay only $1.50" "$1.50" "$1.50" "$11.50")
        ((sxpath `(// (table 3) // (tr 4) // *text*)) sxml))
      (test-equal "line 5"
        '("1980-03-20" "Payment" "inv >90 payment" "$8.00" "1980-01-13"
          "Invoice" "inv >90 payment" "pay only $2.00" "$2.00" "$2.00" "$11.50")
        ((sxpath `(// (table 3) // (tr 5) // *text*)) sxml))
      (test-equal "line 6"
        '("1980-05-13" "1980-05-13" "Invoice" "$6.75" "$14.75" "UNPAID"
          "$6.75" "$6.75")
        ((sxpath `(// (table 3) // (tr 6) // *text*)) sxml))
      (test-equal "line 7"
        '("1980-06-22" "1980-06-22" "Credit Note" "CN" "$11.75" "UNPAID"
          "-$3.00" "$3.00")
        ((sxpath `(// (table 3) // (tr 7) // *text*)) sxml))
      (test-equal "line 8"
        '("1980-06-24" "1980-06-24" "Invoice" "$28.00" "$39.75" "1980-06-26"
          "Payment" "$28.00" "$1.00" "$1.00")
        ((sxpath `(// (table 3) // (tr 8) // *text*)) sxml))
      (test-equal "line 9"
        ' ("1980-06-25" "Credit Note" "$27.00" "$27.00" "$27.00")
        ((sxpath `(// (table 3) // (tr 9) // *text*)) sxml))
      (test-equal "line 10"
        '("1980-06-25" "1980-06-25" "Credit Note" "$27.00" "$12.75"
          "1980-06-24" "Invoice" "$28.00" "$27.00" "$27.00" "$28.00")
        ((sxpath `(// (table 3) // (tr 10) // *text*)) sxml))
      (test-equal "line 11"
        '("1980-06-26" "Payment" "$11.75" "1980-06-24" "Invoice" "$1.00"
          "$1.00" "$28.00")
        ((sxpath `(// (table 3) // (tr 11) // *text*)) sxml)))
    (test-end "new-customer-report")))
