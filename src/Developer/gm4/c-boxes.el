;;; Boxed comments for C mode.
;;; Copyright (C) 1991, 1992, 1993, 1994 Free Software Foundation, Inc.
;;; Francois Pinard <pinard@iro.umontreal.ca>, April 1991.
;;;
;;; I often refill paragraphs inside C comments, while stretching or
;;; shrinking the surrounding box as needed.  This is a real pain to
;;; do by hand.  Here is the code I made to ease my life on this,
;;; usable from within GNU Emacs.  It would not be fair giving all
;;; sources for a product without also giving the means for nicely
;;; modifying them.
;;;
;;; The function rebox-c-comment adjust comment boxes without
;;; refilling comment paragraphs, while reindent-c-comment adjust
;;; comment boxes after refilling.  Numeric prefixes are used to add,
;;; remove, or change the style of the box surrounding the comment.
;;; Since refilling paragraphs in C mode does make sense only for
;;; comments, this code redefines the M-q command in C mode.  I use
;;; this hack by putting, in my .emacs file:
;;;
;;;	(setq c-mode-hook
;;;	      '(lambda ()
;;;		 (define-key c-mode-map "\M-q" 'reindent-c-comment)))
;;;	(autoload 'rebox-c-comment "c-boxes" nil t)
;;;	(autoload 'reindent-c-comment "c-boxes" nil t)
;;;
;;; The cursor should be within a comment before any of these
;;; commands, or else it should be between two comments, in which case
;;; the command applies to the next comment.  When the command is
;;; given without prefix, the current comment box type is recognized
;;; and preserved.  Given 0 as a prefix, the comment box disappears
;;; and the comment stays between a single opening `/*' and a single
;;; closing `*/'.  Given 1 or 2 as a prefix, a single or doubled lined
;;; comment box is forced.  Given 3 as a prefix, a Taarna style box is
;;; forced, but you do not even want to hear about those.  When a
;;; negative prefix is given, the absolute value is used, but the
;;; default style is changed.  Any other value (like C-u alone) forces
;;; the default box style.
;;;
;;; I observed rounded corners first in some code from Warren Tucker
;;; <wht@n4hgf.mt-park.ga.us>.

(defvar c-box-default-style 'single "*Preferred style for box comments.")
(defvar c-mode-taarna-style nil "*Non-nil for Taarna team C-style.")

;;; Set or reset the Taarna team's own way for a C style.

(defun taarna-mode ()
  (interactive)
  (if c-mode-taarna-style
      (progn

	(setq c-mode-taarna-style nil)
	(setq c-indent-level 2)
	(setq c-continued-statement-offset 2)
	(setq c-brace-offset 0)
	(setq c-argdecl-indent 5)
	(setq c-label-offset -2)
	(setq c-tab-always-indent t)
	(setq c-box-default-style 'single)
	(message "C mode: GNU style"))

    (setq c-mode-taarna-style t)
    (setq c-indent-level 4)
    (setq c-continued-statement-offset 4)
    (setq c-brace-offset -4)
    (setq c-argdecl-indent 4)
    (setq c-label-offset -4)
    (setq c-tab-always-indent t)
    (setq c-box-default-style 'taarna)
    (message "C mode: Taarna style")))

;;; Return the minimum value of the left margin of all lines, or -1 if
;;; all lines are empty.

(defun buffer-left-margin ()
  (let ((margin -1))
    (goto-char (point-min))
    (while (not (eobp))
      (skip-chars-forward " \t")
      (if (not (looking-at "\n"))
	  (setq margin
		(if (< margin 0)
		    (current-column)
		  (min margin (current-column)))))
      (forward-line 1))
    margin))

;;; Return the maximum value of the right margin of all lines.  Any
;;; sentence ending a line has a space guaranteed before the margin.

(defun buffer-right-margin ()
  (let ((margin 0) period)
    (goto-char (point-min))
    (while (not (eobp))
      (end-of-line)
      (if (bobp)
	  (setq period 0)
	(backward-char 1)
	(setq period (if (looking-at "[.?!]") 1 0))
	(forward-char 1))
      (setq margin (max margin (+ (current-column) period)))
      (forward-char 1))
    margin))

;;; Add, delete or adjust a C comment box.  If FLAG is nil, the
;;; current boxing style is recognized and preserved.  When 0, the box
;;; is removed; when 1, a single lined box is forced; when 2, a double
;;; lined box is forced; when 3, a Taarna style box is forced.  If
;;; negative, the absolute value is used, but the default style is
;;; changed.  For any other value (like C-u), the default style is
;;; forced.  If REFILL is not nil, refill the comment paragraphs prior
;;; to reboxing.

(defun rebox-c-comment-engine (flag refill)
  (save-restriction
    (let ((undo-list buffer-undo-list)
	  (marked-point (point-marker))
	  (saved-point (point))
	  box-style left-margin right-margin)

      ;; First, find the limits of the block of comments following or
      ;; enclosing the cursor, or return an error if the cursor is not
      ;; within such a block of comments, narrow the buffer, and
      ;; untabify it.

      ;; - insure the point is into the following comment, if any

      (skip-chars-forward " \t\n")
      (if (looking-at "/\\*")
	  (forward-char 2))

      (let ((here (point)) start end temp)

	;; - identify a minimal comment block

	(search-backward "/*")
	(setq temp (point))
	(beginning-of-line)
	(setq start (point))
	(skip-chars-forward " \t")
	(if (< (point) temp)
	    (progn
	      (goto-char saved-point)
	      (error "text before comment's start")))
	(search-forward "*/")
	(setq temp (point))
	(end-of-line)
	(if (looking-at "\n")
	    (forward-char 1))
	(setq end (point))
	(skip-chars-backward " \t\n")
	(if (> (point) temp)
	    (progn
	      (goto-char saved-point)
	      (error "text after comment's end")))
	(if (< end here)
	    (progn
	      (goto-char saved-point)
	      (error "outside any comment block")))

	;; - try to extend the comment block backwards

	(goto-char start)
	(while (and (not (bobp))
		    (progn (previous-line 1)
			   (beginning-of-line)
			   (looking-at "[ \t]*/\\*.*\\*/[ \t]*$")))
	  (setq start (point)))

	;; - try to extend the comment block forward

	(goto-char end)
	(while (looking-at "[ \t]*/\\*.*\\*/[ \t]*$")
	  (forward-line 1)
	  (beginning-of-line)
	  (setq end (point)))

	;; - narrow to the whole block of comments

	(narrow-to-region start end))

      ;; Second, remove all the comment marks, and move all the text
      ;; rigidly to the left to insure the left margin stays at the
      ;; same place.  At the same time, recognize and save the box
      ;; style in BOX-STYLE.

      (let ((previous-margin (buffer-left-margin))
	    actual-margin)

	;; - remove all comment marks

	(goto-char (point-min))
	(replace-regexp "^\\([ \t]*\\)/\\*" "\\1  ")
	(goto-char (point-min))
	(replace-regexp "^\\([ \t]*\\)|" "\\1 ")
	(goto-char (point-min))
	(replace-regexp "\\(\\*/\\||\\)[ \t]*" "")
	(goto-char (point-min))
	(replace-regexp "\\*/[ \t]*/\\*" " ")

	;; - remove the first and last dashed lines

	(setq box-style 'plain)
	(goto-char (point-min))
	(if (looking-at "^[ \t]*-*[.\+\\]?[ \t]*\n")
	    (progn
	      (setq box-style 'single)
	      (replace-match ""))
	  (if (looking-at "^[ \t]*=*[.\+\\]?[ \t]*\n")
	      (progn
		(setq box-style 'double)
		(replace-match ""))))
	(goto-char (point-max))
	(previous-line 1)
	(beginning-of-line)
	(if (looking-at "^[ \t]*[`\+\\]?*[-=]+[ \t]*\n")
	    (progn
	      (if (eq box-style 'plain)
		  (setq box-style 'taarna))
	      (replace-match "")))

	;; - remove all spurious whitespace

	(goto-char (point-min))
	(replace-regexp "[ \t]+$" "")
	(goto-char (point-min))
	(if (looking-at "\n+")
	    (replace-match ""))
	(goto-char (point-max))
	(skip-chars-backward "\n")
	(if (looking-at "\n\n+")
	    (replace-match "\n"))
	(goto-char (point-min))
	(replace-regexp "\n\n\n+" "\n\n")

	;; - move the text left is adequate

	(setq actual-margin (buffer-left-margin))
	(if (not (= previous-margin actual-margin))
	    (indent-rigidly (point-min) (point-max)
			    (- previous-margin actual-margin))))

      ;; Third, select the new box style from the old box style and
      ;; the argument, choose the margins for this style and refill
      ;; each paragraph.

      ;; - modify box-style only if flag is defined

      (if flag
	  (setq box-style
		(cond ((eq flag 0) 'plain)
		      ((eq flag 1) 'single)
		      ((eq flag 2) 'double)
		      ((eq flag 3) 'taarna)
		      ((eq flag '-) (setq c-box-default-style 'plain) 'plain)
		      ((eq flag -1) (setq c-box-default-style 'single) 'single)
		      ((eq flag -2) (setq c-box-default-style 'double) 'double)
		      ((eq flag -3) (setq c-box-default-style 'taarna) 'taarna)
		      (t c-box-default-style))))

      ;; - compute the left margin

      (setq left-margin (buffer-left-margin))

      ;; - temporarily set the fill prefix and column, then refill

      (untabify (point-min) (point-max))

      (if refill
	  (let ((fill-prefix (make-string left-margin ? ))
		(fill-column (- fill-column
				(if (memq box-style '(single double)) 4 6))))
	    (fill-region (point-min) (point-max))))

      ;; - compute the right margin after refill

      (setq right-margin (buffer-right-margin))

      ;; Fourth, put the narrowed buffer back into a comment box,
      ;; according to the value of box-style.  Values may be:
      ;;    plain: insert between a single pair of comment delimiters
      ;;    single: complete box, overline and underline with dashes
      ;;    double: complete box, overline and underline with equal signs
      ;;    taarna: comment delimiters on each line, underline with dashes

      ;; - move the right margin to account for left inserts

      (setq right-margin (+ right-margin
			    (if (memq box-style '(single double))
				2
			      3)))

      ;; - construct the box comment, from top to bottom

      (goto-char (point-min))
      (cond ((eq box-style 'plain)

	     ;; - construct a plain style comment

	     (skip-chars-forward " " (+ (point) left-margin))
	     (insert (make-string (- left-margin (current-column)) ? )
		     "/* ")
	     (end-of-line)
	     (forward-char 1)
	     (while (not (eobp))
	       (skip-chars-forward " " (+ (point) left-margin))
	       (insert (make-string (- left-margin (current-column)) ? )
		       "   ")
	       (end-of-line)
	       (forward-char 1))
	     (backward-char 1)
	     (insert "  */"))
	    ((eq box-style 'single)

	     ;; - construct a single line style comment

	     (indent-to left-margin)
	     (insert "/*")
	     (insert (make-string (- right-margin (current-column)) ?-)
		     "-.\n")
	     (while (not (eobp))
	       (skip-chars-forward " " (+ (point) left-margin))
	       (insert (make-string (- left-margin (current-column)) ? )
		       "| ")
	       (end-of-line)
	       (indent-to right-margin)
	       (insert " |")
	       (forward-char 1))
	     (indent-to left-margin)
	     (insert "`")
	     (insert (make-string (- right-margin (current-column)) ?-)
		     "*/\n"))
	    ((eq box-style 'double)

	     ;; - construct a double line style comment

	     (indent-to left-margin)
	     (insert "/*")
	     (insert (make-string (- right-margin (current-column)) ?=)
		     "=\\\n")
	     (while (not (eobp))
	       (skip-chars-forward " " (+ (point) left-margin))
	       (insert (make-string (- left-margin (current-column)) ? )
		       "| ")
	       (end-of-line)
	       (indent-to right-margin)
	       (insert " |")
	       (forward-char 1))
	     (indent-to left-margin)
	     (insert "\\")
	     (insert (make-string (- right-margin (current-column)) ?=)
		     "*/\n"))
	    ((eq box-style 'taarna)

	     ;; - construct a Taarna style comment

	     (while (not (eobp))
	       (skip-chars-forward " " (+ (point) left-margin))
	       (insert (make-string (- left-margin (current-column)) ? )
		       "/* ")
	       (end-of-line)
	       (indent-to right-margin)
	       (insert " */")
	       (forward-char 1))
	     (indent-to left-margin)
	     (insert "/* ")
	     (insert (make-string (- right-margin (current-column)) ?-)
		     " */\n"))
	    (t (error "unknown box style")))

      ;; Fifth, retabify, restore the point position, then cleanup the
      ;; undo list of any boundary since we started.

      ;; - retabify before left margin only (adapted from tabify.el)

      (goto-char (point-min))
      (while (re-search-forward "^[ \t][ \t][ \t]*" nil t)
	(let ((column (current-column))
	      (indent-tabs-mode t))
	  (delete-region (match-beginning 0) (point))
	  (indent-to column)))

      ;; - restore the point position

      (goto-char (marker-position marked-point))

      ;; - remove all intermediate boundaries from the undo list

      (if (not (eq buffer-undo-list undo-list))
	  (let ((cursor buffer-undo-list))
	    (while (not (eq (cdr cursor) undo-list))
	      (if (car (cdr cursor))
		  (setq cursor (cdr cursor))
		(rplacd cursor (cdr (cdr cursor))))))))))

;;; Rebox a C comment without refilling it.

(defun rebox-c-comment (flag)
  (interactive "P")
  (rebox-c-comment-engine flag nil))

;;; Rebox a C comment after refilling.

(defun reindent-c-comment (flag)
  (interactive "P")
  (rebox-c-comment-engine flag t))
